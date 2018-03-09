import matplotlib.pyplot as plt


def plot():
    one_MB_data = [1.14, 4.54, 6.44, 8.17, 7.99]
    ten_MB_data = [1.13, 5.56, 10.2, 34.0, 43.8]
    hundred_MB_data = [1.14, 5.69, 10.8, 49.0, 88.0]
    improvement_rate = [1, 5, 10, 50, 100]
    one_MB_data_improvement_rate = [item / one_MB_data[0] for item in one_MB_data]
    ten_MB_data_improvement_rate = [item / ten_MB_data[0] for item in ten_MB_data]
    hundred_MB_data_improvement_rate = [item / hundred_MB_data[0] for item in hundred_MB_data]

    plt.loglog(improvement_rate, improvement_rate, label='Equal Improvement in FCT and Bandwidth')
    plt.loglog(improvement_rate, one_MB_data_improvement_rate, label='1MB', marker='o')
    plt.loglog(improvement_rate, ten_MB_data_improvement_rate, label='10MB', marker='o')
    plt.loglog(improvement_rate, hundred_MB_data_improvement_rate, label='100MB', marker='o')

    plt.legend(loc='best')
    plt.xlabel('Relative Bandwidth Improvement (log)')
    plt.ylabel('Relative FCT Improvement (log)')

    plt.show()


plot()
