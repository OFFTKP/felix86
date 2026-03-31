# How to make a new release because you forgot
0. Make sure you're in the commit you want the release to be in. It must be a commit in master
1. Make a new tag: `git tag year.month`, give it the name `felix86 year.month`
2. Push tags: `git push origin --tags`
3. Download the release artifact (linux_artifact) and name it as `felix86.year.month.zip`, upload in release
4. Add the release in `https://cdn.felix86.com/releases/meta.txt` with artifact url (up to 5 lines, newest at top)