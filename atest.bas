print "You are in a room."

main:
w$ = input "What will you do?"
if w$ = "look" then gosub look
else if w$ = "exit" then exit
else print "Invalid input"
goto main

look:
print "You are in a room. There isn't much around."
return
