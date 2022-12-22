#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
extern volatile pte_t uvpt[];     // VA of "virtual page table"
extern volatile pde_t uvpd[];     // VA of current page directory

bool cow_w(pte_t pt){
    bool c_o_w =	pt&PTE_COW;
    bool w = pt&PTE_W;
    return c_o_w | w;
}
bool cow(pte_t pt){
    bool c_o_w =	pt&PTE_COW;
    return c_o_w;
}
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
// 自定义的页面错误处理程序(如果错误页面是写时复制)
// 映射到我们自己的私有可写副本中
static void
pgfault(struct UTrapframe *utf)
{

    void *addr = (void *) utf->utf_fault_va;
    uint32_t err = utf->utf_err;
    int r;
    // Check that the faulting access was (1) a write, and (2) to a
    // copy-on-write page.  If not, panic.
    // Hint:
    //   Use the read-only page table mappings at uvpt
    //   (see <inc/memlayout.h>).
    // 检查错误访问是	[写访问&是写时复制页面]
    //	如果不是, 那么就panic吧, 没办法修复

    // LAB 4: Your code here.
    pte_t pt = uvpt[PGNUM(addr)];
    if( ( err & FEC_WR ) == 0 ||  (!cow(pt)) || ( uvpd[PDX(addr)] & PTE_P )==0 || (uvpt[PGNUM(addr)] & PTE_P)==0 )
        panic("Neither the fault is a write nor COW page. \n");
    // Allocate a new page, map it at a temporary location (PFTEMP),
    // copy the data from the old page to the new page, then move the new
    // page to the old page's address.
    // Hint:
    //   You should make three system calls.
    // 分配一个新页, 将其映射到一个临时位置(PFTEMP),将数据从旧页复制到新页
    // Hint: 你需要进行三次系统调用
    // 1、sys_page_alloc:  为PFTEMP分配一个物理页
    // 2、memmove使得PFTEMP指向的物理页复制addr指向的物理页
    // 3、sys_page_map: 把PFTEMP的物理页映射 复制到  addr处
    // 4、sys_page_unmap: 把PFTEMP的映射取消
    // LAB 4: Your code here.
    if( ( r = sys_page_alloc(0,UTEMP,PTE_P|PTE_U|PTE_W) )<0 )
        panic("sys_page_alloc: %e",r );
    addr = ROUNDDOWN(addr,PGSIZE);
    memmove((void*)UTEMP,(const void*)addr,PGSIZE);
    if( ( r = sys_page_map(0,UTEMP,0,addr,PTE_P|PTE_U|PTE_W) )<0 )
        panic("sys_page_map: %e",r );
    if( ( r = sys_page_unmap(0,UTEMP) )<0 )
        panic("sys_page_unmap: %e",r );
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  
// (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
// 将虚拟页面pn(地址pn*PGSIZE)映射到目标envid的相同虚拟地址.
// 	如果页是可写的或写时复制的, 则必须在写时复制创建新的映射，然后我们的映射也必须标记为写时复制
//	如果页面是可写的或写时复制的，则必须创建新的映射，然后我们的映射也必须标记为写时复制。
//	练习: 如果这个函数之前就已经是写时复制, 为什么我们需要再次标记为写时复制? 
static int
duppage(envid_t envid, unsigned pn)
{
    int r;
    void* va = (void*)(pn*PGSIZE);
    pte_t pt = uvpt[pn];
    int perm = PGOFF(pt);
    bool share = perm&PTE_SHARE;
    if( pn == PGNUM(UXSTACKTOP)-1 ){ // 这是异常栈空间,直接分配给子进程
        if( ( r = sys_page_alloc(envid,va,PTE_P|PTE_U|PTE_W) )<0 ) //给子进程分配空间
            panic("sys_page_alloc: %e",r );
        return 0;
    }
    if( share ){
        if( (r = sys_page_map(0,va,envid,va,PTE_SYSCALL )) < 0 )
            panic("sys_page_map: %e",r );
    }else if( cow_w(pt) ){
        if( (r = sys_page_map(0,va,envid,va,PTE_P|PTE_U|PTE_COW ) ) < 0 )
            panic("sys_page_map: %e",r );
        if( (r = sys_page_map(0,va,0,va,PTE_P|PTE_U|PTE_COW) ) < 0 )
            panic("sys_page_map: %e",r );
    }else{
        if( (r = sys_page_map(0,va,envid,va,PTE_U|PTE_P )) < 0 )
            panic("sys_page_map: %e",r );
    }
    return 0;
}

// 写时复制功能的fork
// 设置页面异常处理程序, 并创建一个子进程
//	将我们的地址空间和也错误处理程序复制到子进程
//	然后将子进程标记为runnable并返回.
//  出错时请panic
// Hint:
//	使用uvpd,uvpt和duppage
// 	记得修复子进程中的thisenv
//	两个用户异常栈不应标记为写时复制, 你需要为子进程的user异常栈分配新页面
envid_t
fork(void)
{
    // LAB 4: Your code here.
    //set_pgfault_handler(0,pgfault);
    set_pgfault_handler(pgfault);
    envid_t envid;
    envid = sys_exofork();
    int r;
    if (envid < 0)
        panic("sys_exofork: %e", envid);
    if (envid == 0) {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }
    extern unsigned char end[];
    int i;
    for(i = PGNUM(UTEXT); i<PGNUM(UTOP) ; i++){
        int addr = i*PGSIZE;
        if ((uvpd[PDX(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_P) && (uvpt[PGNUM(addr)] & PTE_U))
            duppage(envid,i);
    }
    // duppage(envid,PGNUM(&i) );

    if ((r = sys_page_alloc(envid, (void *)(UXSTACKTOP - PGSIZE), PTE_P|PTE_U|PTE_W)))
        panic("sys_page_alloc:%e", r);


    extern void _pgfault_upcall();
    sys_env_set_pgfault_upcall( envid,_pgfault_upcall );

    if( (r = sys_env_set_status(envid,ENV_RUNNABLE)<0 ) ){
        panic("sys_env_set_status: %e", r);
    }
    return envid;
}

// Challenge!
int
sfork(void)
{
    panic("sfork not implemented");
    return -E_INVAL;
}