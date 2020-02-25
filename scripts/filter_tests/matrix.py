import sys
import csv

# 1 construct cpp->(map from test to percentage) mapping cpp2tests
cpp2tests = {}
# 2 and cpp->(minimal percentage coverage)
cpp2minimal = {}
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

            if not cpp in cpp2tests:
                cpp2tests[cpp] = {}
                cpp2minimal[cpp] = 101

            cpp2tests[cpp][file] = percent

            if percent < cpp2minimal[cpp]:
                cpp2minimal[cpp] = percent
    n_tests += 1

# 3 for cpp that are touched in every test delete occurrences equal to "minimal"

for cpp in cpp2tests.keys():
    if len(cpp2tests[cpp]) == n_tests:
        filtered = {}

        for test in cpp2tests[cpp].keys():
            if cpp2tests[cpp][test] != cpp2minimal[cpp]:
                filtered[test] = cpp2tests[cpp][test]

        cpp2tests[cpp] = filtered

# 3 print tests for every file
for cpp in cpp2tests.keys():
    print(cpp, end=" ")
    for test in cpp2tests[cpp]:
        print(test, end=" ")

    print()

print(n_tests)

# print specific test's adjusted coverage - for debugging
with open("RlpTests/EmptyArrayList.txt") as fp:
    reader = csv.reader(fp, delimiter=' ')
    for line in reader:
        cpp = line[0]
        percent = float(line[1])
        lines = int(line[2])

        print(str(percent), str(cpp2minimal[cpp]))
        if percent != cpp2minimal[cpp]:
            print(line)
