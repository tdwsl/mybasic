goto start

printRoom:
print name$
print desc$
return

start:
name$ = "Doorway"
desc$ = "You are in your doorway."
gosub printRoom
