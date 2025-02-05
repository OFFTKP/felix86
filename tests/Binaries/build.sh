for file in **/*.c; do
    gcc -o ${file%.c}.out $file
done
