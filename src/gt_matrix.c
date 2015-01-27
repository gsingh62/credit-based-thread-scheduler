#include <stdio.h>
#include <unistd.h>
#include <linux/unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sched.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>
#include <assert.h>

#include "gt_include.h"


#define ROWS 512
#define COLS ROWS
#define SIZE COLS

#define NUM_CPUS 2
#define NUM_GROUPS NUM_CPUS
#define PER_GROUP_COLS (SIZE/NUM_GROUPS)

#define NUM_THREADS 32
#define PER_THREAD_ROWS (SIZE/NUM_THREADS)


/* A[SIZE][SIZE] X B[SIZE][SIZE] = C[SIZE][SIZE]
 * Let T(g, t) be thread 't' in group 'g'. 
 * T(g, t) is responsible for multiplication : 
 * A(rows)[(t-1)*SIZE -> (t*SIZE - 1)] X B(cols)[(g-1)*SIZE -> (g*SIZE - 1)] */

/*typedef struct matrix
{
	int m[SIZE][SIZE];

	int rows;
	int cols;
	unsigned int reserved[2];
} matrix_t;*/
int counter = 0;
int scheduler_type=-1;
typedef struct matrix
{
	int * m;
	int rows;
	int cols;
	unsigned int reserved[2];
} matrix_t;


typedef struct __uthread_arg
{
	matrix_t *_A, *_B, *_C;
	unsigned int reserved0;
	int matrix_size;
	unsigned int tid;
	unsigned int gid;
	int start_row; /* start_row -> (start_row + PER_THREAD_ROWS) */
	int start_col; /* start_col -> (start_col + PER_GROUP_COLS) */
	int credit;
	
}uthread_arg_t;
	
struct timeval tv1;

static int * generate_matrixhelper(int *m, int val, int size)
{	//printf("generate matrix helper\n");
	m = (int*)malloc(sizeof(int) * size * size);
	int * curr = m;
	int i;
	for(i = 0; i < (size * size); i++)
	{	val +=1;
		*curr = val;
		curr++;
	}

	return m;
}

static int * generate_matrixhelperzero(int *m, int val, int size)
{
	m = (int*)malloc(sizeof(int) * size * size);
	int * curr = m;
	int i;
	for(i = 0; i < (size * size); i++)
	{	
		*curr = val;
		curr++;
	}

	return m;
}
static void generate_matrix(matrix_t *mat, int val, int size)
{	//printf("generate matrix\n");
	mat->rows=size;
	mat->cols=size;
	if(val==0)
	mat->m=generate_matrixhelperzero(mat->m,val,size);
	else mat->m=generate_matrixhelper(mat->m,val,size);
}

static void * uthread_mulmat(void *p)
{	//printf("uthread_mulmat\n");
	counter++;



#define ptr ((uthread_arg_t *)p)

	int col = 0, rows = 0, rows_A = 0, col_A = 0, rows_B = 0, col_B = 0, size = ptr->matrix_size;
	unsigned int cpuid;
	struct timeval tv2;

	//cpuid = kthread_cpu_map[kthread_apic_id()]->cpuid;
	//fprintf(stderr, "\nThread(id:%d, group:%d, cpu:%d) started",ptr->tid, ptr->gid, cpuid);

	int * curr_A = ptr->_A->m;
	int * curr_B = ptr->_B->m;
	int * curr_C = ptr->_C->m;

	while(rows < size)
	{
		while(col < size)
		{
			while(col_A < size && rows_B < size)
			{
				*(curr_C + col + rows * size) += *(curr_A + col_A + rows_A * size) * *(curr_B + col_B +rows_B * size);
				col_A++;
				rows_B++;
			}

	if(counter%50==0 && scheduler_type ==1){
		return gt_yield_func();
	}
			col++;	
			col_B = col;
			col_A = 0;
			rows_A = rows;
			rows_B = 0;	
		}
		
		rows++;
		col = 0;
		rows_A = rows;
		col_A = 0;
		rows_B = 0;
		col_B = 0;		
	}

	gettimeofday(&tv2,NULL);
	//fprintf(stderr, "\nThread(id:%d, group:%d, cpu:%d) finished (TIME : %lu s and %lu us)",
			//ptr->tid, ptr->gid, cpuid, (tv2.tv_sec - tv1.tv_sec), (tv2.tv_usec - tv1.tv_usec));
#undef ptr
	return 0;
}

static uthread_arg_t * generate_uthread_argument_list(uthread_arg_t * uarg, matrix_t * A, matrix_t * B,matrix_t * C, unsigned int tid, int matrix_size, int credit)
{
	uarg->_A = A;
	uarg->_B = B;
	uarg->_C = C;
	uarg->tid =tid;
	uarg->matrix_size = matrix_size;
	if(matrix_size ==32 && credit == 100)
		uarg->gid = 0;
	else if(matrix_size == 64 && credit == 100)
		uarg->gid = 1;
	else if(matrix_size == 128 && credit == 100)
		uarg->gid = 2;
	else if(matrix_size == 256 && credit == 100)
		uarg->gid = 3;
	else if(matrix_size == 32 && credit == 75)
		uarg->gid = 4;
	else if(matrix_size == 64 && credit == 75)
		uarg->gid = 5;
	else if(matrix_size == 128 && credit == 75)
		uarg->gid = 6;
	else if(matrix_size == 256 && credit == 75)
		uarg->gid = 7;
	else if(matrix_size == 32 && credit == 50)
		uarg->gid = 8;
	else if(matrix_size == 64 && credit == 50)
		uarg->gid = 9;
	else if(matrix_size == 128 && credit == 50)
		uarg->gid = 10;
	else if(matrix_size == 256 && credit == 50)
		uarg->gid = 11;
	else if(matrix_size == 32 && credit == 25)
		uarg->gid = 12;
	else if(matrix_size == 64 && credit == 25)
		uarg->gid = 13;
	else if(matrix_size == 128 && credit == 25)
		uarg->gid = 14;
	else if(matrix_size == 256 && credit == 25)
		uarg->gid = 15;
	uarg->credit = credit;
	uarg->start_row = 0;
	uarg->start_col = 0;
	return uarg;	
}

matrix_t A, B, C;

uthread_arg_t uargs[NUM_THREADS];
uthread_t utids[NUM_THREADS];
int main()
{	//printf("main Function Entered\n");
	uthread_arg_t *uarg;
	int tid = 0;
	uarg = &uargs[0];
	int size, j, k, l, m;
	int credit =100;
	matrix_t A,B,C;

	
	/*Unit Test to confirm whether the following functions work: generate_matrix, print_matrix,uthread_mulmat
	generate_matrix(&A,2, 4);
	generate_matrix(&B,3, 4);
	generate_matrix(&C,0, 4);
	uarg = generate_uthread_argument_list(&uargs[0], &A, &B, &C,0, size, CREDIT_25);
	print_matrix(A.m,A.rows);
	printf("\n");
	print_matrix(B.m,B.rows);
	printf("\n");
	print_matrix(C.m,C.cols);
	uthread_mulmat(uarg);
	printf("\n");
	print_matrix(C.m,C.cols);*/
	
	printf("Choose your scheduler:\n(1) Credit Scheduler\n(2) O(1) Priority Scheduler\n");
	scanf("%d",&scheduler_type); /*1 for O(1) Priority Scheduler and 2 for Credit Scheduler*/
	gtthread_app_init(scheduler_type);
	//printf("exited gtthread_app_init\n");
	gettimeofday(&tv1,NULL);

	while(credit >= 25)
	{
		if(credit==100)
		{
			size = 32;
			while(size <= 256)
			{
				//initialize 32 matrices for processing by uthreads with 25 credits and size 32, 64, 128, 256
				for(j = 1; j < 9 ; j++)
				{	
					matrix_t A, A_res;
					generate_matrix(&A, j, size);
					//print_matrix(A.m,size);
					generate_matrix(&A_res, 0, size);
					uarg = generate_uthread_argument_list(&uargs[j-1], &A, &A, &A_res,0, size,credit);	
					uthread_create(&utids[j-1], uthread_mulmat, uarg,uarg->gid, credit);	
				}
				size = size * 2;
			}

			credit = 75;
			//credit = 101;
			continue;		
		}
		else if(credit==75)
		{
			size = 32;
			while(size <= 256)
			{
				//initialize 32 matrices for processing by uthreads with 50 credits and size 32, 64, 128, 256
				for(k = 1; k < 9 ; k++)
				{	
					matrix_t B, B_res;
					generate_matrix(&B, k, size);
					generate_matrix(&B_res, 0, size);
					uarg = generate_uthread_argument_list(&uargs[k+7], &B, &B, &B_res,0, size,credit);	
					uthread_create(&utids[k+7], uthread_mulmat, uarg,uarg->gid, credit);	
				}
				size = size * 2;			
			}
			credit=50;
			continue;
		}
		else if(credit==50)
		{
			size = 32;
			while(size <= 256)
			{
				//initialize 32 matrices for processing by uthreads with 75 credits and size 32, 64, 128, 256
				for(l = 1; l < 9 ; l++)
				{	
					matrix_t B, B_res;
					generate_matrix(&B, l, size);
					generate_matrix(&B_res, 0, size);
					uarg = generate_uthread_argument_list(&uargs[l+15], &B, &B, &B_res,0, size,credit);	
					uthread_create(&utids[l+15], uthread_mulmat, uarg,uarg->gid,credit);	
				}
				size = size * 2;
			}
			credit=25;
			continue;
		}

		else if(credit==25)
		{
			size = 32;
			while(size <= 256)
			{
				//initialize 8 matrices for doing size 256
				for(m = 1; m < 9 ; m++)
				{	
					matrix_t B, B_res;
					generate_matrix(&B, m, size);
					generate_matrix(&B_res, 0, size);
					uarg = generate_uthread_argument_list(&uargs[m+23], &B, &B, &B_res,0, size,credit);	
					uthread_create(&utids[m+23], uthread_mulmat, uarg,uarg->gid,credit);	
				}
				size = size * 2;
			}
			break;
		}		
	}

//	int * kthread_time_taken = gtthread_app_exit();
	//printf("about to enter gtthread_app_exit\n");
	gtthread_app_exit(0);
	//print_averages(kthread_time_taken);
	//print_standard_deviations(kthread_time_taken);

	// print_matrix(&C);
	// fprintf(stderr, "********************************");
	return(0);
}
#if 0
int main()
{
	uthread_arg_t *uarg;
	int inx;


	gtthread_app_init();

	init_matrices();

	gettimeofday(&tv1,NULL);

	for(inx=0; inx<NUM_THREADS; inx++)
	{
		uarg = &uargs[inx];
		uarg->_A = &A;
		uarg->_B = &B;
		uarg->_C = &C;

		uarg->tid = inx;

		uarg->gid = (inx % NUM_GROUPS);

		uarg->start_row = (inx * PER_THREAD_ROWS);
#ifdef GT_GROUP_SPLIT
		/* Wanted to split the columns by groups !!! */
		uarg->start_col = (uarg->gid * PER_GROUP_COLS);
#endif

		uthread_create(&utids[inx], uthread_mulmat, uarg, uarg->gid);
	}

	gtthread_app_exit();

	// print_matrix(&C);
	// fprintf(stderr, "********************************");
	return(0);
}
#endif
