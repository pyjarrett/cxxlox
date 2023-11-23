# C++ Lox Bytecode Virtual Machine

A C++ implementation of the bytecode interpreter from "Crafting Interpreters"
by Robert Nystrom.

# Philosophy

This uses some elements of the C++ standard library, but tries to stay close
to the philosophy of "doing things from scratch" that the book uses.

For example, that means I use `<iostream>` and `std::format`, but wrote a
simple resizable array type instead of using `std::vector`.  It also means
using C-style tagged unions instead of `std::variant`.

I have never written the bytecode interpreter before, so I'm doing it as
close to the book in a more type-safe C++ way, and then refactoring into
more idiomatic C++ as I go.  This won't ever be a production bytecode
interpreter, but it should eventually show my C++ version of how the book
does things.