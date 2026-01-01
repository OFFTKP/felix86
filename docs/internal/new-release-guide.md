# How to make a new release because you forgot
0. Make sure you're in the commit you want the release to be in. It must be a commit in master
1. Make a new tag: `git tag 25.07`. If a message is required, make it `felix86 year.month`
2. Push tags: `git push origin --tags`
3. Download the release artifact (linux_artifact) and name it as `felix86.year.month.zip`
4. Add the release version in `cdn.felix86.com/releases/meta.txt` so it shows up in installer script