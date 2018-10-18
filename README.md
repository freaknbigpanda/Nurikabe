# Nurikabe

Simple Nurikabe solver. See https://en.wikipedia.org/wiki/Nurikabe_(puzzle) for more infomation.

This solution doesn't use any guessing at all so grids that cannot be solved deterministically (i.e. the grids on the wikipedia page) can't be fully solved using this solver.

Current TODO list:
1. Performance optimization, performance can be improved at least 4x over the current implementation without using multithreading
2. Add multithreading
3. Add guessing so that all grids can be solved
