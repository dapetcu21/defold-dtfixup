#define EXTENSION_NAME dtfixup
#define LIB_NAME "dtfixup"
#define MODULE_NAME "dtfixup"
#define SAMPLE_COUNT 60
#define SKIP_COUNT 5
#define MIN_SIGNIFICANT_SAMPLES 10
#define EPSILON 0.01

#define DLIB_LOG_DOMAIN LIB_NAME

#include <dmsdk/sdk.h>

#ifdef DM_PLATFORM_WINDOWS

#include <Windows.h>
#include <algorithm>
#include <cmath>

struct LuaCallbackInfo {
    LuaCallbackInfo() : m_L(0), m_Callback(LUA_NOREF), m_Self(LUA_NOREF) {}
    lua_State *m_L;
    int m_Callback;
    int m_Self;
};

struct Sample {
    long long real;
    double dt, factor, deviation;
};

static dmConfigFile::HConfig appConfig;
static LuaCallbackInfo callback;
static long long realReference;
static long long realFrequency;
static Sample samples[SAMPLE_COUNT];
static int currentSample;

static int dtfixup_init(lua_State *L) {
    if (!dmConfigFile::GetInt(appConfig, "display.variable_dt", 0)) {
        return 0;
    }

    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    int cb = dmScript::Ref(L, LUA_REGISTRYINDEX);

    callback.m_L = dmScript::GetMainThread(L);
    callback.m_Callback = cb;

    dmScript::GetInstance(L);

    callback.m_Self = dmScript::Ref(L, LUA_REGISTRYINDEX);

    currentSample = -1;

    static LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    realFrequency = frequency.QuadPart;

    return 0;
}

static double average(const Sample samples[], int end) {
    long long real = 0;
    double dt = 0.0;
    for (int i = 0; i < end; i++) {
        real += samples[i].real;
        dt += samples[i].dt;
    }
    return (double) real / (double)realFrequency / dt;
}

static bool cmp_deviation(const Sample &a, const Sample &b) {
    return a.deviation < b.deviation;
}

static int dtfixup_update(lua_State *oL)
{
    if (!callback.m_L || currentSample >= SAMPLE_COUNT) {
        return 0;
    }

    double dt = luaL_checknumber(oL, 1);

    LARGE_INTEGER realTimeLI;
    QueryPerformanceCounter(&realTimeLI);
    long long realTime = realTimeLI.QuadPart;

    if (currentSample == -1) {
        realReference = realTime;
        currentSample += 1;
        return 0;
    }

    Sample &sample = samples[currentSample];
    sample.real = realTime - realReference;
    sample.dt = dt;
    sample.factor = (double)sample.real / (double)realFrequency / sample.dt;
    realReference = realTime;

    if (currentSample >= MIN_SIGNIFICANT_SAMPLES) {
        lua_State *L = callback.m_L;
        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, callback.m_Callback);
        lua_rawgeti(L, LUA_REGISTRYINDEX, callback.m_Self);
        dmScript::SetInstance(L);

        double mean = average(samples, currentSample);
        for (int i = 0; i < currentSample; i++) {
            Sample &s = samples[i];
            s.deviation = fabs(s.factor - mean);
        }
        std::sort(samples, samples + currentSample, cmp_deviation);

        double factor = average(samples, currentSample - SKIP_COUNT);
        if (fabs(factor - 1.0) <= EPSILON) {
            // If it's close enough to 1, the bug probably doesn't manifest
            factor = 1.0;
        }
        lua_pushnumber(L, factor);

        int ret = lua_pcall(L, 1, 0, 0);
        if (ret != 0) {
            dmLogError("Error running callback: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }

        assert(top == lua_gettop(L));
    }

    currentSample += 1;
    return 0;
}

#else

static int dtfixup_init(lua_State *L) {
    return 0;
}

static int dtfixup_update(lua_State *L) {
    return 0;
}

#endif

static const luaL_reg Module_methods[] = {
    {"init", dtfixup_init},
    {"update", dtfixup_update},
    {0, 0}
};

static void LuaInit(lua_State *L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeDtFixup(dmExtension::AppParams* params) {
    #ifdef DM_PLATFORM_WINDOWS
    appConfig = params->m_ConfigFile;
    #endif
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeDtFixup(dmExtension::Params *params)
{
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeDtFixup(dmExtension::Params *params)
{
    #ifdef DM_PLATFORM_WINDOWS
    callback = LuaCallbackInfo();
    #endif
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeDtFixup, 0, InitializeDtFixup, 0, 0, FinalizeDtFixup)
