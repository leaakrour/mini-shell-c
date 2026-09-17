# 🐚 Mini-Shell (C)

A Unix shell interpreter built from scratch in C - process management, pipes, redirections, background jobs, and an embedded Scheme interpreter - developed as part of the Systems course at **Grenoble INP - Ensimag**.

## What it does

Implements a working subset of a real shell (bash/zsh-like):

- **Command execution**: `fork()` + `execvp()` to run arbitrary commands
- **Background jobs** (`&`): commands run without blocking the prompt; `jobs` lists running background processes with PID and command, and detects completion via `waitpid`
- **Pipes**: full multi-command pipelines (`cmd1 | cmd2 | cmd3 | ...`), not limited to a single pipe
- **Redirections**: input/output redirection to files (`< in > out`), with existing files correctly truncated
- **Wildcards & environment variables**: expands globs and `$VAR` references via `wordexp()` (variant: *Jokers et environnement*)
- **CPU time limiting**: a built-in `ulimit N` command caps child process CPU time via `setrlimit` (soft limit at N seconds, hard limit N+5s) - variant: *Limitation du temps de calcul*
- **Embedded Scheme interpreter**: extends the shell with GNU Guile, so Scheme expressions can call shell commands via a custom `(executer "cmd")` procedure - e.g. driving shell commands from a Scheme loop

## Usage

```bash
mkdir build && cd build
cmake ..
make
./ensishell
```
```
$ ls -R / | egrep "^to" | egrep ".jpg$" | gzip -c | gzip -cd | less
$ sleep 20 &
$ jobs
$ ulimit 5
$ (display "Hello from Scheme!\n")
```


## Tech stack

C · GNU Readline · GNU Guile (Scheme) · CMake

## Context

Solo project, Systems course, Ensimag 2A. Variant implemented: wildcards/environment variables + CPU time limiting.
