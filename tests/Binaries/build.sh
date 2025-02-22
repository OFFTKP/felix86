for file in **/*.c; do
    gcc -O3 -I./ -o ${file%.c}.out $file
    strip ${file%.c}.out
done
