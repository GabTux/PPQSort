#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<time.h>
#include<omp.h>

#define uint unsigned int

int main()
{
int no_th=32;
size_t n=2e9;
size_t i,j,k,k2,lo,hi;
size_t range=(1ll<<32);
size_t sum,c2[256],c3[256];
size_t pert=(n+no_th-1)/no_th;
size_t perr=(range+no_th-1)/no_th;
omp_set_num_threads(no_th);
unsigned  *count=(unsigned  *) malloc(range*sizeof(unsigned ));
uint *a=(uint *) malloc(n*sizeof(unsigned int));
uint *out=(uint *) malloc(n*sizeof(unsigned int));
if ((count==NULL)||(a==NULL)||(out==NULL))
{
  printf("Alloc error!\n");
  return 1;
}

srand(1);
for(i=0;i<pert;i++)
  a[i]=2*i;//rand()%range; //i
  
  
#pragma omp parallel private(i,j,sum,lo,hi)
{
  i=omp_get_thread_num();
  k=(i<<29);
  if (i>0){  
  lo=i*pert;
  hi=lo+pert;
  if (hi>n) hi=n;
  for(j=lo;j<hi;j++){    
    a[j]=2*j;//(a[j-lo]^k)%range;
  }  
  }
}

/*
for(i=0;i<20;i++)
  printf("i=%lu %u\n",i,a[i]);
*/
printf("Time0 r=%lu n=%lu p=%lu\n",range,n,pert);
fflush(stdout);


double end,start=omp_get_wtime();
#pragma omp parallel for private(i) shared(count)
for(i=0;i<range;i++)
  count[i]=0;
  
end=omp_get_wtime();

printf("Time0=%g r=%lu n=%lu\n",end-start,range,n);
fflush(stdout);  

#pragma omp parallel for private(i) shared(count)
for(i=0;i<n;i++)
{
  uint x=a[i];
  /*
  if (x>=range){
    printf("i=%lu x=%u\n",i,x);
    fflush(stdout);  
  }*/
  /*
  if ((i%100)==0)
    printf("%lu %u\n",i,x);
  */
  #pragma omp atomic update
    (count[x])++;
}

end=omp_get_wtime();

printf("Time1=%g r=%lu n=%lu\n",end-start,range,n);
fflush(stdout);
/*
for(i=0;i<range;i++)
{
  uint x=count[i];
  printf("%lu %u\n",i,x);
}
*/
#pragma omp parallel private(i,j,sum,lo,hi) shared(count,c2)
{
  i=omp_get_thread_num();
  sum=0;  
  lo=i*perr;
  hi=lo+perr;
  if (hi>range) hi=range;
  for(j=lo;j<hi;j++) 
  {   
    sum+=count[j];
    /*
    if ((j%100)==0)
      printf("%lu %lu\n",j,sum);
      */    
  }
  //printf("i=%lu %lu %lu\n",i,lo,hi);  
  c2[i]=sum;  
}

end=omp_get_wtime();
printf("Time2=%g r=%lu n=%lu\n",end-start,range,n);
/*
for(i=0;i<no_th;i++)
  printf("i=%lu %lu\n",i,c2[i]);
*/
fflush(stdout);
 
sum=0;
for(i=0;i<=no_th;i++)
{
  c3[i]=sum;
  sum+=c2[i];
}
for(i=0;i<=no_th;i++)
  printf("i=%lu %lu\n",i,c3[i]);
fflush(stdout);
//c3[no_th]=n;
#pragma omp parallel private(i,j,k,k2,sum,lo,hi) shared(c3,count)
{
  i=omp_get_thread_num();
  sum=c3[i];  
  lo=i*perr;
  hi=lo+perr;
  if (hi>range) hi=range;
  for(j=lo;j<hi;j++)  
  {  
    k=count[j];
    for(k2=0;k2<k;k2++)
      out[sum++]=j;
  }     
  printf("i=%lu sum=%lu\n",i,sum);
  fflush(stdout);
}

end=omp_get_wtime();
printf("Time3=%g r=%lu n=%lu\n",end-start,range,n);
printf("c3=%lu\n",c3[no_th]);
fflush(stdout);
}