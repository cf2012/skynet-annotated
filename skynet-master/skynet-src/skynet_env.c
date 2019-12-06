#include "skynet.h"
#include "skynet_env.h"
#include "spinlock.h"

#include <lua.h>
#include <lauxlib.h>

#include <stdlib.h>
#include <assert.h>

struct skynet_env {
	struct spinlock lock;
	lua_State *L;
};

// 静态全局变量. 作用域: 本文件
static struct skynet_env *E = NULL;


/** 从vm中获取key的值 */
const char * 
skynet_getenv(const char *key) {
	SPIN_LOCK(E)

	lua_State *L = E->L;
	
	lua_getglobal(L, key); // 把全局 key 的值压入栈顶
	const char * result = lua_tostring(L, -1); //  获得栈顶的值
	lua_pop(L, 1); // 删除栈顶1个元素

	SPIN_UNLOCK(E)

	return result;
}

/** 将 vm 中变量key的值设置为 value. 如果key已经有非nil的值, 异常推出 */
void 
skynet_setenv(const char *key, const char *value) {
	SPIN_LOCK(E)
	
	lua_State *L = E->L;
	lua_getglobal(L, key); // 将 key对应的值压入栈顶
	assert(lua_isnil(L, -1)); // key 对应的值应该为 nil
	lua_pop(L,1);
	lua_pushstring(L,value);
	lua_setglobal(L,key);

	SPIN_UNLOCK(E)
}


/** 初始化静态全局变量 E */
void
skynet_env_init() {
	E = skynet_malloc(sizeof(*E));
	SPIN_INIT(E)
	E->L = luaL_newstate();
}
