#include "thumbrt.h"
#include <stdlib.h>
static int nfn; uint32_t last_bad;
static int cmpf(const void*k,const void*e){uint32_t a=*(const uint32_t*)k,b=((const fn_entry_t*)e)->addr;return a<b?-1:a>b;}
// 원본의 ARM 도우미 5개(0x882f0 memset, 0x883a4 memcpy, 0x884dc 64비트 나눗셈, 0x889dc setjmp, 0x889f4 longjmp) 대응.
static int arm_helper(cpu_t*c,uint32_t a){
  uint32_t*r=c->r;
  switch(a){
  case 0x1882f0: CK(r[0],r[2]); memset(c->m+r[0],(int)r[1],r[2]); return 1;
  case 0x1883a4: CK(r[0],r[2]); CK(r[1],r[2]); memmove(c->m+r[0],c->m+r[1],r[2]); return 1;
  case 0x1884dc: { int64_t n=(int64_t)(((uint64_t)r[1]<<32)|r[0]),d=(int64_t)(((uint64_t)r[3]<<32)|r[2]); int64_t q=d?n/d:0,m=d?n%d:n;
    r[0]=(uint32_t)q;r[1]=(uint32_t)((uint64_t)q>>32);r[2]=(uint32_t)m;r[3]=(uint32_t)((uint64_t)m>>32); return 1; }
  }
  return 0;
}
void call_ind(cpu_t*c,uint32_t t){
  if(arm_helper(c,t&~1u))return;
  if(!nfn)while(fn_table[nfn].addr)nfn++;
  uint32_t a=t&~1u;
  const fn_entry_t*e=bsearch(&a,fn_table,nfn,sizeof(fn_entry_t),cmpf);
  if(!e){last_bad=t;c->fault=3;longjmp(c->jb,1);}
  e->fn(c);
}
// 반환: 0 성공, 그 외 fault 코드. regs는 입출력(r0-r15).
int run_fn(uint32_t addr,uint32_t*regs,uint8_t*mem,uint32_t memsz){
  static cpu_t c; memset(&c,0,sizeof c); memcpy(c.r,regs,sizeof c.r); c.m=mem; c.memsz=memsz;
  if(setjmp(c.jb)) return c.fault;
  call_ind(&c,addr);
  if(c.fault)return c.fault;
  memcpy(regs,c.r,sizeof c.r); return 0;
}
