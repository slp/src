/* Invalid initializations.  */
/* { dg-require-effective-target tls } */

extern __thread int i;
__thread int *p = &i;	/* { dg-error "dynamically initialized" } */

extern int f();
__thread int j = f();	/* { dg-error "dynamically initialized" } */

struct S
{
  S();
};
__thread S s;		/* { dg-error "" } two errors here */
