// 기계 번역 코드의 런타임. 메모리는 c->m + 게스트 주소다.
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
typedef struct cpu { uint32_t pc; uint32_t r[16]; int n,z,cf,v; uint8_t *m; uint64_t icount; int fault; jmp_buf jb; uint32_t memsz; } cpu_t;
#define c_ cf
#undef c_
#define SP r[13]
#define LR r[14]
typedef void (*fn_t)(cpu_t*);
typedef struct { uint32_t addr; fn_t fn; } fn_entry_t;
extern const fn_entry_t fn_table[];
void call_ind(cpu_t*c,uint32_t t);

static inline void chk(cpu_t*c,uint32_t a,uint32_t n){ if(a<0x100000u||a+n>c->memsz){c->fault=2;longjmp(c->jb,1);} }
#define CK(a,n) chk(c,(a),(n))
static inline uint32_t R32_(cpu_t*c,uint32_t a){CK(a,4);uint32_t v;memcpy(&v,c->m+a,4);return v;}
static inline uint32_t R16_(cpu_t*c,uint32_t a){CK(a,2);uint16_t v;memcpy(&v,c->m+a,2);return v;}
static inline uint32_t R8_(cpu_t*c,uint32_t a){CK(a,1);return c->m[a];}
static inline void W32_(cpu_t*c,uint32_t a,uint32_t x){CK(a,4);memcpy(c->m+a,&x,4);}
static inline void W16_(cpu_t*c,uint32_t a,uint32_t x){CK(a,2);uint16_t v=x;memcpy(c->m+a,&v,2);}
static inline void W8_(cpu_t*c,uint32_t a,uint32_t x){CK(a,1);c->m[a]=x;}
#define R32(a) R32_(c,(a))
#define R16(a) R16_(c,(a))
#define R8(a) R8_(c,(a))
#define RS16(a) ((uint32_t)(int32_t)(int16_t)R16_(c,(a)))
#define RS8(a) ((uint32_t)(int32_t)(int8_t)R8_(c,(a)))
#define W32(a,x) W32_(c,(a),(x))
#define W16(a,x) W16_(c,(a),(x))
#define W8(a,x) W8_(c,(a),(x))
#define NZ(x) (c->n=(uint32_t)(x)>>31,c->z=(uint32_t)(x)==0)
static inline uint32_t addc(cpu_t*p,uint32_t a,uint32_t b,uint32_t ci){uint64_t u=(uint64_t)a+b+ci;uint32_t r=(uint32_t)u;p->n=r>>31;p->z=r==0;p->cf=u>>32;p->v=((a^r)&(b^r))>>31;return r;}
static inline uint32_t lsl_(cpu_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){if(s<32){p->cf=(a>>(32-s))&1;r=a<<s;}else{p->cf=s==32?a&1:0;r=0;}}p->n=r>>31;p->z=r==0;return r;}
static inline uint32_t lsr_(cpu_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){if(s<32){p->cf=(a>>(s-1))&1;r=a>>s;}else{p->cf=s==32?a>>31:0;r=0;}}p->n=r>>31;p->z=r==0;return r;}
static inline uint32_t asr_(cpu_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){if(s<32){p->cf=(a>>(s-1))&1;r=(uint32_t)((int32_t)a>>s);}else{p->cf=a>>31;r=(uint32_t)((int32_t)a>>31);}}p->n=r>>31;p->z=r==0;return r;}
static inline uint32_t ror_(cpu_t*p,uint32_t a,uint32_t s){s&=255;uint32_t r=a;if(s){s&=31;if(s)r=(a>>s)|(a<<(32-s));p->cf=r>>31;}p->n=r>>31;p->z=r==0;return r;}
#ifdef TRACE_PC
#define TICK(a) (c->pc=(a))
#else
#define TICK(a) do{ if(++c->icount>2000000u){c->fault=4;longjmp(c->jb,1);} }while(0)
#endif
