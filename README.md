# ProjectNoot
Funny game about penguins

## Git 

Download
> https://git-scm.com/docs/git

Manual
> https://training.github.com/downloads/github-git-cheat-sheet/

We use submodules! If after an update the code doesn't compile chances are submodules haven't been cloned or updated.

##  How to clone

Run this command in Git Bash
```
git clone --recurse-submodules git@github.com:BeefFungusGeese/ProjectNoot.git
```

## How to submit work using Git bash

1) Make sure you have created a feature branch for your changes.
Naming convention for features:
>feature/name-of-your-features

Naming convention for bugfixes:
>bugfix/name-of-your-bugfix

2) Stage and commit your work. Commit often, briefly describe what you changed in your commit.

``` 
git add .
git commit -m "description of your changes"
```
3) Upload your changes to the server
``` 
git push origin 
```
4) Make a pull request for the changes to be merged in
5) Other than for management and readme purposes avoid pushing to main branch

##  How to update (best practice)

1) Stage and commit your current changes
2) Fetch changes
```
git fetch origin
```
3) Merge *origin/main* into your working branch using diff tool of choice
4) Update your submodules
```
git submodule update --init --recursive
```

## Using Visual Studio for git

1. Rightclicking into the folder your git repository is in lets you use Visual Studio as a graphical git management tool.
2. Next to Solution explorer you have *Git Changes*, letting you inspect, stage and commit changes. You can also fetch/pull and manage branches at the top of the window.
3. Under *View/Git Repository* you get an overview of the repository. You can initiate a merge by rightlicking on the remote branch and *merge "origin/name' into 'name'*
4. If there are conflicts you can resolve them in the *Git Changes* window. Right clicking on conflicts lets you choose which version to take (for binary files), otherwise double click for diff tool.
