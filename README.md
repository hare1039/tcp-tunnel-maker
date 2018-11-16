# tcp-tunnel-maker
tcp tunnel maker using c++17, coroutine, boost asio

# usage
```
|--------|          |----------------|
| my mac |          | 192.168.1.100  |
| :7000  |          | ./maker -p 300 |
| client ---------->:300             |
|--------|          |----------------|
./maker -p 7000 --via 192.168.1.100:300 ip.hare1039.nctu.me:80
```

In this theme, I can curl `localhost:7000` and retrive infomation from `ip.hare1039.nctu.me:80` via `192.168.1.100`
