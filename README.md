## Example usage

```
client --file testfile.txt --server 127.0.0.1 --smp 1 --port 10003 --logger-log-level client=debug
```

```
server --port 10000 --dir test --smp 1 --logger-log-level server=debug
```

```
python test.py build/client build/server -p 10003 -s 5 -n 4 -v
```