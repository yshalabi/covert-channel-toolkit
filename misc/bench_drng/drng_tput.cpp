#include <thread>
#include <x86intrin.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <future>
#include <math.h>
using namespace std;
#include <immintrin.h>

#ifndef SAMPLE_WIDTH
#define SAMPLE_WIDTH 64
#endif

#if SAMPLE_WIDTH==16
#define RDRAND __builtin_ia32_rdrand16_step
#define RDSEED __builtin_ia32_rdseed_hi_step
#define TYPE uint16_t
#elif SAMPLE_WIDTH==32
#define RDRAND __builtin_ia32_rdrand32_step
#define RDSEED __builtin_ia32_rdseed_si_step
#define TYPE uint32_t
#else
#define RDRAND __builtin_ia32_rdrand64_step
#define RDSEED __builtin_ia32_rdseed_di_step
#define TYPE long long unsigned int //compiler wont compile if uint64_t ... long long unsigned int *can* be 32 bits on platforms, gcc implements this interface with long long..
#endif

#ifndef DRNG_OP
#define DRNG_OP RDSEED
#endif

#ifndef BENCH_TIME
#define BENCH_TIME 4.0
#endif

#ifndef THREADS_PER_NODE
#define THREADS_PER_NODE 44
#endif

#ifndef REPEAT_COUNT
#define REPEAT_COUNT 1
#endif

#ifndef DRNG_OP_TYPE_STR
#define DRNG_OP_TYPE_STR "unknown"
#endif
/*
 * Compile time switches:
 * SAMPLE_WIDTH     -  number of random bits sampled from DRNG
 * DRNG_OP          - [RDRAND,RDSEED] - selects whether to use seeded (i.e. local) or non-seeded (i.e. global)
 * THREADS_PER_NODE - experiment runs over iterations [1, THREADS_PER_NODE]. In iteration i, n=i threads are launched to stress the DRNG. 
 * BENCH_TIME       - BENCH_TIME is the length of time each thread spends hammering the DRNG
 *
 * runtime args:
 * pause_cnt        - (optional) specify number of pause instructions executed after each DRNG request
 */
const float TIME_PER_THREAD = BENCH_TIME;
const int MAX_THREADS       = THREADS_PER_NODE;
const int RUN_COUNT         = REPEAT_COUNT + 1;
const int RECV_SIDE_RNGOP_CNT = 4;
const int RECV_SIDE_PAUSE_CNT = 16;

constexpr int RATE_SHIFT_CNT(const float tpt) {
    return (int) log2(tpt);
} 

float effective_rate(uint64_t count)
{
    return (float)( count >> RATE_SHIFT_CNT(TIME_PER_THREAD));
}
//fix the problem that i assume TPT is power2
const double SEND_PRIME_PERIOD  = 0.5;
const double RECV_PROBE_PERIOD  = 0.5;
const double RECV_SYNC_PERIOD   = 0.5;
const double SEND_SYNC_PERIOD   = 0.5;
void usleep(uint64_t usec)
{
    uint64_t finish = __rdtsc() + usec * 2200;
    while(__rdtsc() < finish)
        continue;
    return;
}
void cycle_delay(uint64_t cycles)
{
	int cycles_high0, cycles_high1, cycles_low0, cycles_low1;
    asm volatile ("cpuid\n\t"
		  "rdtsc\n\t"
		  "mov %%edx, %0\n\t"
		  "mov %%eax, %1\n\t"
		  : "=r" (cycles_high0), "=r" (cycles_low0)
		  :: "%rax", "%rbx", "%rcx", "%rdx");
    asm volatile ("rdtscp\n\t"
		  "mov %%edx, %0\n\t"
		  "mov %%eax, %1\n\t"
		  "cpuid\n\t"
		  : "=r" (cycles_high1), "=r" (cycles_low1)
		  :: "%rax", "%rbx", "%rcx", "%rdx");
	int elapsed = cycles_low1 - cycles_low0;
	if(elapsed < cycles)
		return cycle_delay(cycles-elapsed);
    return;
}

pair<uint64_t,uint64_t> drng_loop(int pause_cnt)
{
    chrono::duration<double> elapsed_time;
    int res;
    TYPE val;
    uint64_t op_count = 0;
    uint64_t fail_count = 0;
    auto start = std::chrono::system_clock::now();
    for(;;) 
    {
        res = DRNG_OP(&val);
        auto done = std::chrono::system_clock::now();
        if(res)
            op_count++;
        else
            fail_count++;
        elapsed_time = done-start;
        if(elapsed_time.count() > TIME_PER_THREAD)
            break;
        //usleep(50);
        for(int i = 0; i < pause_cnt; i++)
            asm volatile("pause");
    }
    return pair<uint64_t,uint64_t>(op_count,fail_count);
}
void bcast_one()
{
    TYPE val;
    DRNG_OP(&val);
    return;
}
void bcast_str(string m, int delay)
{
    using namespace std::chrono;
	auto transmit_one = []() {
		TYPE val;
        high_resolution_clock::time_point start = high_resolution_clock::now();
        high_resolution_clock::time_point done = high_resolution_clock::now();
        auto dur = duration_cast<duration<double>>(done-start);
        do {
            DRNG_OP(&val);	
            done = high_resolution_clock::now();
            dur = duration_cast<duration<double>>(done-start);
        } while(dur.count() <= SEND_PRIME_PERIOD);
		return;
	};
	auto transmit_zero = []() {
        high_resolution_clock::time_point start = high_resolution_clock::now();
        high_resolution_clock::time_point done = high_resolution_clock::now();
        TYPE val;
        auto dur = duration_cast<duration<double>>(done-start);
        do {
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            done = high_resolution_clock::now();
            dur = duration_cast<duration<double>>(done-start);
        } while(dur.count() < SEND_PRIME_PERIOD);
		return;
	};

    for (auto & c : m) 
    {
        short secret    = (short) c;
        short bit       = 1;
        for(int i = 0; i < 8; i++)
        {
            if(secret & 1)
                transmit_one();
            else
                transmit_zero();
            secret = secret >> 1;
        } 
    }
    return;
}

void bcast_char(char c, int delay)
{
    using namespace std::chrono;
	auto transmit_one = []() {
		TYPE val;
        high_resolution_clock::time_point start = high_resolution_clock::now();
        high_resolution_clock::time_point done = high_resolution_clock::now();
        auto dur = duration_cast<duration<double>>(done-start);
        do {
            for(int i = 0; i < 1024; i++)
                DRNG_OP(&val);	
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            asm("pause");
            done = high_resolution_clock::now();
            dur = duration_cast<duration<double>>(done-start);
        } while(dur.count() <= SEND_PRIME_PERIOD);
		return;
	};
	auto transmit_zero = []() {
        high_resolution_clock::time_point start = high_resolution_clock::now();
        high_resolution_clock::time_point done = high_resolution_clock::now();
        auto dur = duration_cast<duration<double>>(done-start);
        do {
            asm("pause");
            done = high_resolution_clock::now();
            dur = duration_cast<duration<double>>(done-start);
        } while(dur.count() < SEND_PRIME_PERIOD);
		return;
	};

    short secret    = (short) c;
    short bit       = 1;
    for(int i = 0; i < 8; i++)
    {
		if(secret & 1)
			transmit_one();
		else
			transmit_zero();
		secret = secret >> 1;
    } 

    return;
}
int determine_delay()
{
    int delay = 9, success = 0;
    int zero;
    TYPE val;
    for(;;) {
        int error = 0;
        for(int b = 0; b < 8; b++)
        {
            zero = 0;
            for(int j = 0; j < 4096; j++) {
				int cycles_high0, cycles_high1, cycles_low0, cycles_low1;
				asm volatile ("cpuid\n\t"
					  "rdtsc\n\t"
					  "mov %%edx, %0\n\t"
					  "mov %%eax, %1\n\t"
					  : "=r" (cycles_high0), "=r" (cycles_low0)
					  :: "%rax", "%rbx", "%rcx", "%rdx");
                zero += DRNG_OP(&val);
				do {
					asm volatile ("rdtscp\n\t"
						  "mov %%edx, %0\n\t"
						  "mov %%eax, %1\n\t"
						  "cpuid\n\t"
						  : "=r" (cycles_high1), "=r" (cycles_low1)
						  :: "%rax", "%rbx", "%rcx", "%rdx");
				} while(cycles_low1 - cycles_low0 < delay);
            }
            if(zero < 2048) {
                error++;
			} 
        }
        if(error)
            delay++;
        else
            break;
    }
    return delay;
}
void recv_str(string m, double * data)
{
    using namespace std::chrono;
    TYPE val;
    int delay = 15;
    int zero, one;
    int j = 0;
    int SAMPLE_THRESH = 2048;
    short v = 0;
    for(auto c : m)
    {
        for(int b = 0; b < 8; b++)
        {
            int cnt = SAMPLE_THRESH;
            high_resolution_clock::time_point start = high_resolution_clock::now();
            while(cnt > 0)
            {
                for(int o = 0; o < RECV_SIDE_RNGOP_CNT; o++)
                    cnt-=DRNG_OP(&val);
                for(int p = 0; p < RECV_SIDE_PAUSE_CNT; p++)
                    asm("pause");
            }
            high_resolution_clock::time_point done = high_resolution_clock::now();
            auto dur = duration_cast<duration<double>>(done-start);
            data[j++] = dur.count();
            while(1) {
                dur = duration_cast<duration<double>>(high_resolution_clock::now()-start);
                if(dur.count() < RECV_PROBE_PERIOD)
                    asm("pause");
                else
                    break;
            }
        }
    }
    j=0;
    for(auto & c : m) {
        int correct_bits = 0;
        int incorrect_bits = 0;
        short received = 0;
        for(int i = 0; i < 8; i++) {
            int sample  = j++;
            int bit     = i; //sample % 8;
            int val     = (c >> bit) & 1;
            double cnt = data[sample];
            int predicted = cnt >= 0.002;
            received |= (predicted << i);
            if(predicted == val)
                correct_bits++;
            else
                incorrect_bits++;
            cout << c << "," << bit << "," << cnt << "," << val << "," << predicted << endl;
        }
        cout << "sent: " << (char) c;
        cout << "--- recevied: " << (char) received;
        cout << "    (hd : " << incorrect_bits << ")" << endl; 
    }
    //correct_bits / (correct_bits+incorrect_bits) << endl;
    return;
}



void recv_char(char c, double * data)
{
    using namespace std::chrono;
    TYPE val;
    int delay = 15;
    int zero, one;
    int j = 0;
    int SAMPLE_THRESH = 1024;
    short v = 0;
    for(int b = 0; b < 8; b++)
    {
        int cnt = SAMPLE_THRESH;
        high_resolution_clock::time_point start = high_resolution_clock::now();
        while(cnt > 0) {
            cnt-=DRNG_OP(&val);
        }
        high_resolution_clock::time_point done = high_resolution_clock::now();
        auto dur = duration_cast<duration<double>>(done-start);
        data[j++] = dur.count();
        while(1) {
            dur = duration_cast<duration<double>>(high_resolution_clock::now()-start);
            if(dur.count() < RECV_PROBE_PERIOD)
                asm("pause");
            else
                break;
        }
    }
    int correct_bits = 0;
    int incorrect_bits = 0;
    short received = 0;
    for(j=0; j<8; j++) {
        int sample  = j;
        int bit     = sample % 8;
        int val     = (c >> bit) & 1;
        double cnt = data[sample];
        int predicted = cnt >= 0.002;
        received |= (predicted << j);
        if(predicted == val)
            correct_bits++;
        else
            incorrect_bits++;
        //cout << bit << "," << cnt << "," << val << "," << predicted << endl;
    }
    cout << "recevied: " << (char) received;
    cout << "   (hd : " << incorrect_bits << ")" << endl; 
    //correct_bits / (correct_bits+incorrect_bits) << endl;
    return;
}


void recv_secret(string m, double * data)
{
    using namespace std::chrono;
    TYPE val;
    int delay = 15;
    int zero, one;
    int j = 0;
    int SAMPLE_THRESH = 2048;
    for(int i = 0; i < m.size(); i++)
    {
        short v = 0;
        for(int b = 0; b < 8; b++)
        {
            int cnt = SAMPLE_THRESH;
            high_resolution_clock::time_point start = high_resolution_clock::now();
            while(cnt > 0) {
                cnt-=DRNG_OP(&val);
            }
            high_resolution_clock::time_point done = high_resolution_clock::now();
            auto dur = duration_cast<duration<double>>(done-start);
            data[j++] = dur.count();
            while(1) {
                dur = duration_cast<duration<double>>(high_resolution_clock::now()-start);
                if(dur.count() < RECV_PROBE_PERIOD)
                    asm("pause");
                else
                    break;
            }
        }
    }
    int correct_bits = 0;
    int incorrect_bits = 0;
    for(j=0; j<8*m.size(); j++) {
        int sample  = j;
        int bit     = sample % 8;
        int symbol  = (sample/8)%m.size();
        int val     = (m[symbol] >> bit) & 1;
        double cnt = data[sample];
        int predicted = cnt >= 0.002;
        if(predicted == val) correct_bits++;
        cout << sample << "," << symbol << "," << bit << "," << cnt << "," << val << "," << predicted << endl;
    }
    cout << "accuracy: " << (double) correct_bits / (correct_bits+incorrect_bits) << endl;
    return;
}

void bcast_secret_str(string m)
{
    cout << "bcast_secret_str: " << m << endl;
    atomic_int done, rdy;
    int THREADS = 4;
    rdy     = 0;
    done    = 0;
    char c = 'c';
	//std::this_thread::sleep_for(1s);
    auto f = [&]() {
        rdy++;
        while(rdy < (THREADS+1))
            ;
        bcast_str(m,1024);
        done++;
    };
    double * data = (double *) malloc (sizeof(double) * 8*m.size());
    auto r = [&]() {
        rdy++;
        while(rdy < (THREADS+1))
            ;
        recv_str(m,data);
        done++;
    };

    using namespace std::chrono;
    rdy     = 0;
    done    = 0;
    vector<future<void>> trojans;
    trojans.reserve(THREADS);
    for(int i = 0; i < THREADS; i++) {
        trojans.emplace_back(async(launch::async, f));
    }
    auto r1 = async(launch::async, r);
    while(done < (THREADS+1))
        asm("pause");
    free(data);
    return;
}

void bcast_secret(string m)
{
    cout << "bcast_secret: " << m << endl;
    atomic_int done, rdy;
    int THREADS = 16;
    rdy     = 0;
    done    = 0;
    char c = 'c';
	//std::this_thread::sleep_for(1s);
    auto f = [&]() {
        rdy++;
        while(rdy < (THREADS+1))
            ;
        bcast_char(c,1024);
        done++;
    };
    double * data = (double *) malloc (sizeof(double) * 8); // m.size());
    auto r = [&]() {
        rdy++;
        while(rdy < (THREADS+1))
            ;
        recv_char(c,data);
        done++;
    };

    using namespace std::chrono;
    for(auto & C : m) {
        cout << "sending: " << C << endl;
        c = C;
        rdy     = 0;
        done    = 0;
        vector<future<void>> trojans;
        trojans.reserve(THREADS);
        for(int i = 0; i < THREADS; i++) {
            trojans.emplace_back(async(launch::async, f));
        }
        auto r1 = async(launch::async, r);
        while(rdy < (THREADS+1))
            ;
        while(done < (THREADS+1))
            asm("pause");
    }
    free(data);
    return;
}
typedef pair<uint64_t, uint64_t> result_t;
constexpr int total_sample_count(const int repeat_count, const int thread_cnt) {
    if(thread_cnt == 0)
        return 0;
    const int SAMPLE_CNT_PER_THREAD = repeat_count;
    const int TOTAL_SAMPLE_CNT      = SAMPLE_CNT_PER_THREAD  * thread_cnt;
    return TOTAL_SAMPLE_CNT + total_sample_count(repeat_count, thread_cnt - 1);
}
int main(int argc, char**argv) {
    int pause_cnt = 0;
    uint64_t sum = 0;
    if(argc >= 2 ) {
        pause_cnt = atoi(argv[1]);
    }
    if(argc == 3)
    {
        for(int i = 0; i < 10; i++)
            bcast_secret("hello, world!");
		return 0;
    }

    if(argc == 4)
    {
        bcast_secret_str("hello, world!");
		return 0;
    }



    const int TOTAL_SAMPLE_COUNT = total_sample_count(RUN_COUNT, MAX_THREADS);
    array<result_t, total_sample_count(RUN_COUNT, MAX_THREADS)> exp_samples;
    int s = 0;


    for(int num_threads = 1; num_threads <= MAX_THREADS; num_threads++) 
    {
        uint64_t minf = (uint64_t) -1; 
        for(int r = 0; r < RUN_COUNT; r++)
        {
            //allocate NUM_THREADS futures
            vector<future<pair<uint64_t,uint64_t>>> drng_stressor_results;
            drng_stressor_results.reserve(num_threads);

            //launch NUM_THREADS async-tasks to stress drng
            for(int i = 0; i < num_threads; i++)
                drng_stressor_results.emplace_back(async(launch::async, drng_loop, pause_cnt));

            //wait on last task launched
            drng_stressor_results.back().wait();

            //move all results into results array
            for(auto & f : drng_stressor_results)
                exp_samples[s++] = move(f.get());
        }
    }

    cout.precision(5);
    cout << scientific;
    int num_threads = 1;
    int sample = 0;
    int batch_size = RUN_COUNT;
    for(auto & [succeed_cnt, failure_cnt] : exp_samples)
    {
        int sample_id       = sample++;
        sample              %= batch_size;
        int tid             = (sample_id) % num_threads;

        auto success_rate   = effective_rate(succeed_cnt);
        auto failure_rate   = effective_rate(failure_cnt);
        auto success_brate  = effective_rate(succeed_cnt*SAMPLE_WIDTH);
        auto failure_brate  = effective_rate(failure_cnt*SAMPLE_WIDTH);

        //output sample description
        cout << DRNG_OP_TYPE_STR << "," << TIME_PER_THREAD << "," << SAMPLE_WIDTH << "," << pause_cnt << "," << num_threads << "," << tid << ",";
        cout << (float) succeed_cnt << "," << (float) success_rate << "," << (float) success_brate << ",";
        cout << (float) failure_cnt << "," << (float) failure_rate << "," << (float) failure_brate << endl; 
        
        bool next_batch_reached = sample == 0;
        if( next_batch_reached ) {
            num_threads++;
            batch_size = num_threads * RUN_COUNT;
        }
    }

}
