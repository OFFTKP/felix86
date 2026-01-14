# How to contribute

Contributions are welcome!

Check out [https://felix86.com/contrib/](https://felix86.com/contrib/) for more information.

## Compiling
To compile, run the following:

```
cmake -B build
cmake --build build -j $(nproc)
```

To install in binfmt_misc, run the following:
```
sudo --preserve-env=HOME ./build/felix86 -b
```
This is not necessary for most games, but for example AppImages need it for setuid bit support.    
If you recompile make sure to register felix86 again by running the same command.    


---

If you want to do felix86 development I recommend you join the discord so we can communicate:    
https://discord.gg/TgBxgFwByU