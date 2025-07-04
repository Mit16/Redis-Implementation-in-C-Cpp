#!/usr/bin/env python3

import shlex
import subprocess
import sys

CASES = r'''
$ ./client zscore asdf n1
(nil)
$ ./client zquery xxx 1 asdf 1 10
(arr) len=0
(arr) end
$ ./client zadd zset 1 n1
(int) 1
$ ./client zadd zset 2 n2
(int) 1
$ ./client zadd zset 1.1 n1
(int) 0
$ ./client zscore zset n1
(dbl) 1.1
$ ./client zquery zset 1 "" 0 10
(arr) len=4
(str) n1
(dbl) 1.1
(str) n2
(dbl) 2
(arr) end
$ ./client zquery zset 1.1 "" 1 10
(arr) len=2
(str) n2
(dbl) 2
(arr) end
$ ./client zquery zset 1.1 "" 2 10
(arr) len=0
(arr) end
$ ./client zrem zset adsf
(int) 0
$ ./client zrem zset n1
(int) 1
$ ./client zquery zset 1 "" 0 10
(arr) len=2
(str) n2
(dbl) 2
(arr) end
# EDGE CASES
$ ./client zadd z1 not-a-number a
(err) 4 expect float
$ ./client zquery z1 0 "" 0 bad
(err) 4 expect int
$ ./client zadd key1 5 test
(int) 1
$ ./client pexpire key1 1000
(int) 1
$ ./client pttl key1
(int) 1000
$ sleep 1
$ ./client pttl key1
(int) -2
$ ./client get key1
(nil)
'''

def normalize(text):
    return [line.strip() for line in text.strip().splitlines() if line.strip()]

def main():
    cmds = []
    expected_outputs = []
    lines = CASES.strip().splitlines()

    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith('$ '):
            cmds.append(line[2:])
            expected_outputs.append('')
        else:
            expected_outputs[-1] += line + '\n'

    if len(cmds) != len(expected_outputs):
        print("Mismatch in number of commands and expected outputs.")
        sys.exit(1)

    passed = 0
    for i, (cmd, expect) in enumerate(zip(cmds, expected_outputs), 1):
        if cmd.startswith("sleep "):
            import time
            delay = float(cmd.split()[1])
            print(f"⏱️  Sleeping for {delay} sec...")
            time.sleep(delay)
            passed += 1
            continue

        try:
            out = subprocess.check_output(shlex.split(cmd), stderr=subprocess.STDOUT, timeout=2).decode('utf-8')
        except subprocess.CalledProcessError as e:
            out = e.output.decode('utf-8')
        except subprocess.TimeoutExpired:
            print(f"⏱️  Timeout: {cmd}")
            continue

        norm_out = normalize(out)
        norm_exp = normalize(expect)

        if norm_out == norm_exp:
            print(f"✅ Test {i:02}: {cmd}")
            passed += 1
        else:
            print(f"❌ Test {i:02} FAILED: {cmd}")
            print("Expected:")
            print(expect)
            print("Got:")
            print(out)
            print("------")

    print(f"\n🧪 {passed}/{len(cmds)} tests passed.")

if __name__ == '__main__':
    main()
