# userprog/args-none
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/userprog/args-none -a args-none --swap-size=4 -- -q  -f run args-none < /dev/null 2> tests/userprog/args-none.errors > tests/userprog/args-none.output

# userprog/read-boundary
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/userprog/read-boundary -a read-boundary -p ../../tests/userprog/sample.txt -a sample.txt --swap-size=4 -- -q  -f run read-boundary < /dev/null 2> tests/userprog/read-boundary.errors > tests/userprog/read-boundary.output

# userprog/write-bad-ptr
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/userprog/write-bad-ptr -a write-bad-ptr -p ../../tests/userprog/sample.txt -a sample.txt --swap-size=4 -- -q  -f run write-bad-ptr < /dev/null 2> tests/userprog/write-bad-ptr.errors > tests/userprog/write-bad-ptr.output

# vm/pt-bad-read
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/pt-bad-read -a pt-bad-read -p ../../tests/vm/sample.txt -a sample.txt --swap-size=4 -- -q  -f run pt-bad-read < /dev/null 2> tests/vm/pt-bad-read.errors > tests/vm/pt-bad-read.output

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


# vm/page-linear
pintos -v -k -T 3 --qemu  --filesys-size=2 -p tests/vm/page-linear -a page-linear --swap-size=4 -- -q  -f run page-linear < /dev/null 2> tests/vm/page-linear.errors > tests/vm/page-linear.output

# vm/page-parallel
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/page-parallel -a page-parallel -p tests/vm/child-linear -a child-linear --swap-size=4 -- -q  -f run page-parallel < /dev/null 2> tests/vm/page-parallel.errors > tests/vm/page-parallel.output

# vm/page-merge-seq
pintos -v -k -T 4 --qemu  --filesys-size=2 -p tests/vm/page-merge-seq -a page-merge-seq -p tests/vm/child-sort -a child-sort --swap-size=4 -- -q  -f run page-merge-seq < /dev/null 2> tests/vm/page-merge-seq.errors > tests/vm/page-merge-seq.output

# vm/page-merge-par
pintos -v -k -T 4 --qemu  --filesys-size=2 -p tests/vm/page-merge-par -a page-merge-par -p tests/vm/child-sort -a child-sort --swap-size=4 -- -q  -f run page-merge-par < /dev/null 2> tests/vm/page-merge-par.errors > tests/vm/page-merge-par.output

# vm/page-merge-stk
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/page-merge-stk -a page-merge-stk -p tests/vm/child-qsort -a child-qsort --swap-size=4 -- -q  -f run page-merge-stk < /dev/null 2> tests/vm/page-merge-stk.errors > tests/vm/page-merge-stk.output

# vm/page-merge-mm
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/page-merge-mm -a page-merge-mm -p tests/vm/child-qsort-mm -a child-qsort-mm --swap-size=4 -- -q  -f run page-merge-mm < /dev/null 2> tests/vm/page-merge-mm.errors > tests/vm/page-merge-mm.output



# vm/mmap-read
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/mmap-read -a mmap-read -p ../../tests/vm/sample.txt -a sample.txt --swap-size=4 -- -q  -f run mmap-read < /dev/null 2> tests/vm/mmap-read.errors > tests/vm/mmap-read.output

# vm/mmap-close
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/mmap-close -a mmap-close -p ../../tests/vm/sample.txt -a sample.txt --swap-size=4 -- -q  -f run mmap-close < /dev/null 2> tests/vm/mmap-close.errors > tests/vm/mmap-close.output

# vm/mmap-write
pintos -v -k -T 2 --qemu  --filesys-size=2 -p tests/vm/mmap-write -a mmap-write --swap-size=4 -- -q  -f run mmap-write < /dev/null 2> tests/vm/mmap-write.errors > tests/vm/mmap-write.output
