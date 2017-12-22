# userprog/args-none
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/userprog/args-none -a args-none --swap-size=4 -- -q  -f run args-none < /dev/null 2> tests/userprog/args-none.errors > tests/userprog/args-none.output

# userprog/read-boundary
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/userprog/read-boundary -a read-boundary -p ../../tests/userprog/sample.txt -a sample.txt --swap-size=4 -- -q  -f run read-boundary < /dev/null 2> tests/userprog/read-boundary.errors > tests/userprog/read-boundary.output

# userprog/write-bad-ptr
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/userprog/write-bad-ptr -a write-bad-ptr -p ../../tests/userprog/sample.txt -a sample.txt --swap-size=4 -- -q  -f run write-bad-ptr < /dev/null 2> tests/userprog/write-bad-ptr.errors > tests/userprog/write-bad-ptr.output

# vm/pt-grow-stack
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-grow-stack -a pt-grow-stack --swap-size=4 -- -q  -f run pt-grow-stack < /dev/null 2> tests/vm/pt-grow-stack.errors > tests/vm/pt-grow-stack.output

# vm/pt-grow-pusha
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-grow-pusha -a pt-grow-pusha --swap-size=4 -- -q  -f run pt-grow-pusha < /dev/null 2> tests/vm/pt-grow-pusha.errors > tests/vm/pt-grow-pusha.output

# vm/pt-grow-bad
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-grow-bad -a pt-grow-bad --swap-size=4 -- -q  -f run pt-grow-bad < /dev/null 2> tests/vm/pt-grow-bad.errors > tests/vm/pt-grow-bad.output

# vm/pt-big-stk-obj
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-big-stk-obj -a pt-big-stk-obj --swap-size=4 -- -q  -f run pt-big-stk-obj < /dev/null 2> tests/vm/pt-big-stk-obj.errors > tests/vm/pt-big-stk-obj.output

# vm/pt-grow-stk-sc
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-grow-stk-sc -a pt-grow-stk-sc --swap-size=4 -- -q  -f run pt-grow-stk-sc < /dev/null 2> tests/vm/pt-grow-stk-sc.errors > tests/vm/pt-grow-stk-sc.output

# vm/pt-write-code2
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-write-code2 -a pt-write-code2 -p ../../tests/vm/sample.txt -a sample.txt --swap-size=4 -- -q  -f run pt-write-code2 < /dev/null 2> tests/vm/pt-write-code2.errors > tests/vm/pt-write-code2.output
