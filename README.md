# KayShell
A simple shell for linux

Command, Process management, Foreground/Background running, I/O redirection supported.

## Files:
	KayShell Excutable linux file
	add	Excutable linux sample file. Take an integer as parameter and display that number plus 2
	counter Excutable linux sample file. Count numbers from 1 with 1 second delay
	kshell.h	Header file for project
	kshell.c	Main source file, manage process and IO redirection
	destructor.c	OOP Implement
	parser.c	Command text parser
	builtin.c	Built-in commands. kill;jobs;bg;fg
	CMakeLists.txt	CMake file

## Features:
	A linux shell without tube. Implicit commands supported. i.e. both OK with ./add  add
