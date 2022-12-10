// User-level IPC library routines

#include <inc/lib.h>

// Receive a value via IPC and return it.
// If 'pg' is nonnull, then any page sent by the sender will be mapped at
// 	that address.
// If 'from_env_store' is nonnull, then store the IPC sender's envid in
// 	*from_env_store.
// If 'perm_store' is nonnull, then store the IPC sender's page permission
// 	in *perm_store (this is nonzero iff a page was successfully
// 	transferred to 'pg').
// If the system call fails, then store 0 in *fromenv and *perm (if
// 	they're nonnull) and return the error.
// Otherwise, return the value sent by the sender

// Hint:
//   Use 'thisenv' to discover the value and who sent it.
//   If 'pg' is null, pass sys_ipc_recv a value that it will understand
//   as meaning "no page".  (Zero is not the right value, since that's
//   a perfectly valid place to map a page.)
/*
通过IPC接收一个值并返回它。
如果` pg `是非空的，那么发送端发送的任何页面都会被映射到
这个地址。
如果` from_env_store `非null，则将IPC发送者的envid存储在其中
* from_env_store。
如果` perm_store `非null，那么存储IPC发送者的页面权限
在*perm_store中(如果页成功存储，该值为非零)
转移到'pg')。
如果系统调用失败，则将0存储在*fromenv和*perm中(如果系统调用失败，则将0保存在*fromenv和*perm中)
它们是非null的)，并返回错误。
否则，返回发送方发送的值

提示:
使用` thisenv `来发现这个值以及发送它的人。
如果` pg `为null，传递给sys_ipc_recv一个它能理解的值
意思是“没有页”。(零不是正确的值，因为它是
这是映射页面的完美位置。)
*/
int32_t
ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
{
	int r;
	if( pg != NULL ){  // 接收页面
		if( ( r = sys_ipc_recv(pg) )<0 ){
			if( from_env_store != NULL )	*from_env_store = 0;
			if( perm_store != NULL )	*perm_store = 0;
			return r;
		}
		if( perm_store != NULL )
			*perm_store = thisenv->env_ipc_perm;
	}else{
		sys_ipc_recv((void*)UTOP);
	}
	if( from_env_store != NULL ){
		*from_env_store = thisenv->env_ipc_from;
	}
	if( perm_store != NULL )
		*perm_store = thisenv->env_ipc_perm;
	return thisenv->env_ipc_value;
}

// Send 'val' (and 'pg' with 'perm', if 'pg' is nonnull) to 'toenv'.
// This function keeps trying until it succeeds.
// It should panic() on any error other than -E_IPC_NOT_RECV.

// Hint:
//   Use sys_yield() to be CPU-friendly.
//   If 'pg' is null, pass sys_ipc_try_send a value that it will understand
//   as meaning "no page".  (Zero is not the right value.)
/*
将` val `(以及` pg `和` perm `，如果` pg `不为null)发送到` toenv `。
这个函数会一直尝试，直到成功。
除-E_IPC_NOT_RECV之外的任何错误都应该使用panic()。

提示:
使用sys_yield()对cpu是友好的。
如果` pg `为null，传递给sys_ipc_try_send一个它能理解的值
意思是“没有页”。(零不是正确的值。)
*/
void
ipc_send(envid_t to_env, uint32_t val, void *pg, int perm)
{
	// LAB 4: Your code here.
	if( pg == NULL )
		pg = (void*)UTOP;
	int r;
	while( true ){
		r = sys_ipc_try_send(to_env,val,pg,perm);
		if( r==0 )	return;
		else if( r==-E_IPC_NOT_RECV )	sys_yield();
		else	panic("sys_ipc_try_send: %e",r );
	}
}

// Find the first environment of the given type.  We'll use this to
// find special environments.
// Returns 0 if no such environment exists.
/*
//找到给定类型的第一个环境我们用它来
//查找特殊的环境
//如果不存在这样的环境，则返回0
*/
envid_t
ipc_find_env(enum EnvType type)
{
	int i;
	for (i = 0; i < NENV; i++)
		if (envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
