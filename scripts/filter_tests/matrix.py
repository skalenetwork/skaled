import sys
import csv

# 1 construct cpp->(map from test to percentage) mapping cpp2tests
cpp2tests = {}
# 2 and cpp->(some stats on percent)
n_tests = 0
for file in sys.stdin:
    file = file.strip()
    if not file.endswith('.txt'):
        sys.stderr.write("Skipping " + file + "\n")
        continue
    file = file[0:-4]

    with open(file+".txt") as fp:
        reader = csv.reader(fp, delimiter = ' ')
        for line in reader:
            cpp = line[0]
            percent = float(line[1])
            lines = int(line[2])

            if lines < 20:
                continue

            if not cpp in cpp2tests:
                cpp2tests[cpp] = {}

            cpp2tests[cpp][file] = percent

    n_tests += 1

# 3 for each cpp that delete occurrences less than average

for cpp in cpp2tests.keys():
    count = len(cpp2tests[cpp])

    print(cpp, end=" ")

    test2val = cpp2tests[cpp]
    arr = sorted(test2val, key = lambda k: test2val[k], reverse = True) 

    sum = 0
    max_value = 0
    after_break = False

    # get top tests with non-repeating percentages!
    for i in range(len(arr)):
        skip = False
        if (i>0 and test2val[arr[i]] == test2val[arr[i-1]]) or (i<len(arr)-1 and test2val[arr[i]] == test2val[arr[i+1]]):
            skip = True
        if not skip:
            sum += test2val[arr[i]]

        if test2val[arr[i]] < max_value / 5:
            after_break = True

        if not after_break and not skip:
            if max_value == 0:
                max_value = test2val[arr[i]]
            print(f"{arr[i]}", end=" ")
            #print(f"{arr[i]}[{test2val[arr[i]]}]", end=" ")
        else:
            pass #print(f"{test2val[arr[i]]}", end=" ")

#        if sum >= 100:
#            after_break = True

    print()

print(n_tests)

# print specific test's adjusted coverage - for debugging
#with open("RlpTests/EmptyArrayList.txt") as fp:
#    reader = csv.reader(fp, delimiter=' ')
#    for line in reader:
#        cpp = line[0]
#        percent = float(line[1])
#        lines = int(line[2])
#
#        print(str(percent), str(cpp2minimal[cpp]))
#        if percent != cpp2minimal[cpp]:
#            print(line)
