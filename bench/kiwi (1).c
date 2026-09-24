#include <string.h>
#include "../engine/db.h"
#include "../engine/variant.h"
#include "bench.h"
#include <sys/time.h>

#define DATAS ("testdb")

void _write_test(long int count, int r)
{
	int i;
	double cost;
	long long start,end;
	Variant sk, sv;
	DB* db;

	char key[KSIZE + 1];
	char val[VSIZE + 1];
	char sbuf[1024];

	memset(key, 0, KSIZE + 1);
	memset(val, 0, VSIZE + 1);
	memset(sbuf, 0, 1024);

	db = db_open(DATAS);

	start = get_ustime_sec();
	for (i = 0; i < count; i++) {
		if (r)
			_random_key(key, KSIZE);
		else
			snprintf(key, KSIZE, "key-%d", i);
		fprintf(stderr, "%d adding %s\n", i, key);
		snprintf(val, VSIZE, "val-%d", i);

		sk.length = KSIZE;
		sk.mem = key;
		sv.length = VSIZE;
		sv.mem = val;

		db_add(db, &sk, &sv);
		if ((i % 10000) == 0) {
			fprintf(stderr,"random write finished %d ops%30s\r", 
					i, 
					"");

			fflush(stderr);
		}
	}

	db_close(db);

	end = get_ustime_sec();
	cost = end -start;

	printf(LINE);
	printf("|Random-Write	(done:%ld): %.6f sec/op; %.1f writes/sec(estimated); cost:%.3f(sec);\n"
		,count, (double)(cost / count)
		,(double)(count / cost)
		,cost);	
}

void _read_test(long int count, int r)
{
	int i;
	int ret;
	int found = 0;
	double cost;
	long long start,end;
	Variant sk;
	Variant sv;
	DB* db;
	char key[KSIZE + 1];

	db = db_open(DATAS);
	start = get_ustime_sec();
	for (i = 0; i < count; i++) {
		memset(key, 0, KSIZE + 1);

		/* if you want to test random write, use the following */
		if (r)
			_random_key(key, KSIZE);
		else
			snprintf(key, KSIZE, "key-%d", i);
		fprintf(stderr, "%d searching %s\n", i, key);
		sk.length = KSIZE;
		sk.mem = key;
		ret = db_get(db, &sk, &sv);
		if (ret) {
			//db_free_data(sv.mem);
			found++;
		} else {
			INFO("not found key#%s", 
					sk.mem);
    	}

		if ((i % 10000) == 0) {
			fprintf(stderr,"random read finished %d ops%30s\r", 
					i, 
					"");

			fflush(stderr);
		}
	}

	db_close(db);

	end = get_ustime_sec();
	cost = end - start;
	printf(LINE);
	printf("|Random-Read	(done:%ld, found:%d): %.6f sec/op; %.1f reads /sec(estimated); cost:%.3f(sec)\n",
		count, found,
		(double)(cost / count),
		(double)(count / cost),
		cost);
}

// ===============================================================================================
typedef struct {
	int thread_id;
	long int total_operations;
	int write_percentage;
	DB* db;
} ThreadArgs;


pthread_mutex_t stats_mutex;      
long long global_total_adds = 0;
long long global_total_gets = 0;
double global_add_time = 0.0;
double global_get_time = 0.0;


double get_current_time_sec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void* mixed_worker_thread(void* arg) {
	ThreadArgs* args = (ThreadArgs*)arg;
	int writes = 0;
	int reads = 0;
	
	unsigned int seed = time(NULL) + args->thread_id;

    Variant sv_read;
    sv_read.length = 0;
    sv_read.mem = malloc(1); 

	for (long int i = 0; i < args->total_operations; i++) {
		char key[KSIZE + 1];
		memset(key, 0, KSIZE + 1);

		int random_key_num = rand_r(&seed) % 100000;
		snprintf(key, KSIZE, "key-%d", random_key_num);
		
		Variant sk;
		sk.length = KSIZE;
		sk.mem = key;

		int op_choice = rand_r(&seed) % 100;
		
		if (op_choice < args->write_percentage) {
			
			char val[VSIZE + 1];
			memset(val, 0, VSIZE + 1);
			snprintf(val, VSIZE, "val-%d", random_key_num);
			
			Variant sv_write;
			sv_write.length = VSIZE;
			sv_write.mem = val;
            
            double t_start = get_current_time_sec();
			db_add(args->db, &sk, &sv_write);
            double t_end = get_current_time_sec();
            
            pthread_mutex_lock(&stats_mutex);
            global_total_adds++;
            global_add_time += (t_end - t_start);
            pthread_mutex_unlock(&stats_mutex);
            
			writes++;
		} else {
			
            double t_start = get_current_time_sec();
			db_get(args->db, &sk, &sv_read);
            double t_end = get_current_time_sec();
            
            pthread_mutex_lock(&stats_mutex);
            global_total_gets++;
            global_get_time += (t_end - t_start);
            pthread_mutex_unlock(&stats_mutex);
            
			reads++;
		}
	}
	
    free(sv_read.mem);
	printf("Thread %d finished: %d writes, %d reads\n", args->thread_id, writes, reads);
	pthread_exit(NULL);
}

void _mixed_test(long int count, int num_threads, int write_percentage)
{
	DB* db;
	pthread_t threads[num_threads];
	ThreadArgs args[num_threads];
	long long start, end;
	double cost;

	long int ops_per_thread = count / num_threads;

    pthread_mutex_init(&stats_mutex, NULL);
    global_total_adds = 0;
    global_total_gets = 0;
    global_add_time = 0.0;
    global_get_time = 0.0;

	db = db_open(DATAS);
	start = get_ustime_sec();

	for (int i = 0; i < num_threads; i++) {
		args[i].thread_id = i + 1;
		args[i].total_operations = ops_per_thread;
		args[i].write_percentage = write_percentage;
		args[i].db = db;
		
		pthread_create(&threads[i], NULL, mixed_worker_thread, &args[i]);
	}

	for (int i = 0; i < num_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	db_close(db);

	end = get_ustime_sec();
	cost = end - start;

	printf(LINE);
	printf("|Mixed-Workload (done:%ld, threads:%d, writes:%d%%): %.6f sec/op; %.1f ops/sec(estimated); cost:%.3f(sec)\n",
		count, num_threads, write_percentage,
		(double)(cost / count),
		(double)(count / cost),
		cost);
        
    
    printf(LINE);
    printf("=== DETAILED PERFORMANCE STATISTICS ===\n");
    if (global_total_adds > 0) {
        printf("Total ADDs          : %lld\n", global_total_adds);
        printf("Average ADD Time    : %.6f sec\n", global_add_time / global_total_adds);
        printf("ADD Throughput      : %.1f ops/sec\n", global_total_adds / global_add_time);
    }
    printf("--------------------------------------\n");
    if (global_total_gets > 0) {
        printf("Total GETs          : %lld\n", global_total_gets);
        printf("Average GET Time    : %.6f sec\n", global_get_time / global_total_gets);
        printf("GET Throughput      : %.1f ops/sec\n", global_total_gets / global_get_time);
    }
    printf(LINE);

    
    pthread_mutex_destroy(&stats_mutex);
}
