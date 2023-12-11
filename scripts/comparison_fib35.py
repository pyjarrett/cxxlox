import time


def fib(n):
    if n < 2:
        return n
    return fib(n - 1) + fib(n - 2)


start = time.time()
print(fib(35))
end = time.time()
print(end - start)
