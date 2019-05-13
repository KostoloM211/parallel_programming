#!/common/python-2.7.15/bin/python
import sys
import os
import numpy as np

from ctypes import *

# Обертка над функцией для одноразового выполнения
def run_once(f):
    def wrapper(*args, **kwargs):
        if not wrapper.has_run:
            wrapper.has_run = True
            return f(*args, **kwargs)
    wrapper.has_run = False
    return wrapper

# Превращение данных в С-указатель для передачи в dll
c_float_p = POINTER(c_float)
def pointer(f):
    return f.ctypes.data_as(c_float_p)

# Глобальные переменные
mx = 640
my = 480
num_iter = 100
rowsCount = 0
rank = c_int(0)
size = c_int(0)

# Пути чтения и записи
inputFile = '/home/kosta3ov/course/input2'
outputFile = '/home/kosta3ov/course/output_p'

# Подгрузка dll
mpi_lib_path = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), "mpilib.so"))
mpi_lib = CDLL(mpi_lib_path)

# Инициализация MPI
mpi_lib.mpiinit()

# Кто Я / Сколько Нас
mpi_lib.ranksize(byref(rank), byref(size))


# Чтение порции данных
def readLocalData():
    global rowsCount

    m1 = (mx - 2) / size.value + 2
    rem = (mx - 2) % size.value
    m2 = m1 + 1 if rem > 0 else m1

    startRow = 0

    if rank.value < rem:
        startRow = rank.value * (m2 - 2)
        rowsCount = m2
    else:
        startRow = rem * (m2 - 2) + (rank.value - rem) * (m1 - 2)
        rowsCount = m1

    fp = open(inputFile, 'rb')
    fp.seek(sizeof(c_float) * startRow * my)

    arr = np.fromfile(fp, dtype=c_float, count=rowsCount * my).reshape((rowsCount, my))
    copyArr = np.copy(arr)
    return arr, copyArr

# Обработка массива
# def processData(startRow, endRow):
#     for i in range(startRow, endRow):
#         newData[i, 1:my - 1] = (newData[i, 1:my - 1] + newData[i, 2:my] + newData[i, 0:my - 2] + newData[i + 1, 1:my - 1] + newData[i - 1, 1:my - 1]) * 0.25

def processData(startRow, endRow):
    newData[startRow:endRow, 1:my - 1] = (newData[startRow:endRow, 1:my - 1] + newData[startRow:endRow, 0:my - 2] + newData[startRow:endRow, 2:my] + newData[startRow - 1:endRow - 1, 1:my - 1] + newData[startRow + 1:endRow + 1, 1:my - 1]) * 0.25

# Декоратор для выполнения создания файла для записи (выполнить один раз)
@run_once
def createOutputFile():
    print 'output was created'
    fp = open(outputFile, "wb")
    fp.close()

# Запись байтов в файл со строки start до end массива data
def writeBytes(f, start, end):
    bw = data[start:end, 0:my].tobytes() 
    f.write(bw)
    print "written", (end - start), 'rows', '=', len(bw), 'bytes'

def writeToOutputFile(fp):
    if rank.value == 0:
        writeBytes(fp, 0, 1)

    writeBytes(fp, 1, rowsCount - 1)
    
    if rank.value == size.value - 1:
        writeBytes(fp, rowsCount - 1, rowsCount)

def writeResult():
    fp = open(outputFile, "ab")

    if size.value == 1:
        fp.write(data.tobytes())
        fp.close()
        return

    if rank.value == 0:
        writeToOutputFile(fp);
        mpi_lib.sendSynchromessage(1)

    elif rank.value == size.value - 1:
        mpi_lib.recvSynchromessage(size.value - 2)
        writeToOutputFile(fp);

    else:
        mpi_lib.recvSynchromessage(rank.value - 1)
        writeToOutputFile(fp);
        mpi_lib.sendSynchromessage(rank.value + 1)

    # print data

    fp.close()


# Итерация решения уравнения
def solve():
    global newData, data

    countRequests = 2 if rank.value > 0 and rank.value < size.value - 1 else 1

    if rank.value != 0:
        calculatedRow = 1
        processData(calculatedRow, calculatedRow + 1)

    mpi_lib.allocRequests(countRequests)

    if rank.value == 0:
        create = run_once(createOutputFile)
        create()
        mpi_lib.recieveFromDown(pointer(newData), 1)
        processData(1, rowsCount - 2)

    elif rank.value == size.value - 1:
        mpi_lib.sendToUp(pointer(newData), size.value - 2)
        processData(2, rowsCount - 1);
    else:
        mpi_lib.sendToUp(pointer(newData), rank.value - 1)
        mpi_lib.recieveFromDown(pointer(newData) , rank.value + 1)
        processData(2, rowsCount - 2);
    
    mpi_lib.waitRequests()

    mpi_lib.allocRequests(countRequests)

    if rank.value != size.value - 1:
        lastCalculatedRow = rowsCount - 2
        processData(lastCalculatedRow, lastCalculatedRow + 1)
    
    if rank.value == 0:
        mpi_lib.sendToDown(pointer(newData), 1)
    
    elif rank.value == size.value - 1:
        mpi_lib.receiveFromUp(pointer(newData), size.value - 2)

    else:
        mpi_lib.receiveFromUp(pointer(newData), rank.value - 1)
        mpi_lib.sendToDown(pointer(newData), rank.value + 1)
    
    mpi_lib.waitRequests()

    newData, data = data, newData




# Чтение
newData, data = readLocalData()
# Отправка данных о размере прочтенных данных
mpi_lib.sendDimensions(rowsCount, my)

# Засекаем время
mpi_lib.tick.restype = c_double
t1 = mpi_lib.tick()

#  Итерации
if rank.value == 0 and size.value == 1:
    for i in range(num_iter):
        processData(1, mx - 1)
        newData, data = data, newData
else:
    for i in range(num_iter):
        solve()

# Время решения
t2 = mpi_lib.tick()
print "time =", t2 - t1

# Запись результата
writeResult()
mpi_lib.finalize()






