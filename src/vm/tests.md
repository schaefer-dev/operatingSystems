# vm/pt-grow-stack
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-grow-stack -a pt-grow-stack --swap-size=4 -- -q  -f run pt-grow-stack < /dev/null 2> tests/vm/pt-grow-stack.errors > tests/vm/pt-grow-stack.output

# vm/pt-grow-pusha
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-grow-pusha -a pt-grow-pusha --swap-size=4 -- -q  -f run pt-grow-pusha < /dev/null 2> tests/vm/pt-grow-pusha.errors > tests/vm/pt-grow-pusha.output

# vm/pt-grow-bad
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-grow-bad -a pt-grow-bad --swap-size=4 -- -q  -f run pt-grow-bad < /dev/null 2> tests/vm/pt-grow-bad.errors > tests/vm/pt-grow-bad.output

# vm/pt-big-stk-obj
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-big-stk-obj -a pt-big-stk-obj --swap-size=4 -- -q  -f run pt-big-stk-obj < /dev/null 2> tests/vm/pt-big-stk-obj.errors > tests/vm/pt-big-stk-obj.output
