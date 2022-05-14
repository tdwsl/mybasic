dim a(5)

for i = 1 to 5
  a(i) = i*100
next

a(3) = 253
a(5) = 8038

for i = 1 to 5
  print a(i)
next

print

dim s$(1+1)
s$(1) = "Hello"
s$(2) = "world!"

for i = 1 to 2
  print s$(i)
next
