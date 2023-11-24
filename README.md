# HTTP Server

High-performance C++ HTTP server using epoll

## Build
```bash
make
```

## Test with wrk
### Install wrk
```bash
sudo apt-get install wrk
```
### Run tests

Start the HTTP server:
```bash
./bin/http_server
```
Run wrk to test:
```bash
wrk -t8 -c1000 -d10s http://localhost:8080
```

### Results
```
Running 10s test @ http://localhost:8080
  8 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     7.76ms    4.55ms  77.66ms   92.75%
    Req/Sec    16.85k     4.18k   27.72k    83.33%
  1332273 requests in 10.03s, 87.67MB read
Requests/sec: 132769.84
Transfer/sec:      8.74MB
```

```
Running 10s test @ http://localhost:8080
  8 threads and 1000 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.37ms    1.49ms  24.47ms   76.90%
    Req/Sec    19.37k     1.84k   28.12k    76.12%
  1545112 requests in 10.08s, 101.67MB read
Requests/sec: 153326.04
Transfer/sec:     10.09MB
```