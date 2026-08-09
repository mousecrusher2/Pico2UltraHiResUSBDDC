import matplotlib.pyplot as plt
import numpy as np
from scipy import interpolate, signal

# フィルタの基本条件
FS = 384000  # アップサンプリング前の周波数[Hz]
UPSAMPLING_RATIO = 4  # アップサンプリング倍率

# フィルタのパラメータ
WP = FS * 0.055  # 通過域遮断周波数[Hz]
WS = FS * 0.57  # 阻止域遮断周波数[Hz]
GPASS = 0.5  # 通過域最大損失量[dB]
GSTOP = 140  # 阻止域最小減衰量[dB]
FTYPE = "cheby2"  # フィルタタイプ(チェビシェフ2型)

# シミュレーション条件
TIME_IMPULSE_SIMU = 0.0012  # インパルス応答解析時間[s]


def print_filter_params(param_, filter_type):
    """フィルター係数をC言語の配列定義形式で出力する。"""
    param = np.copy(param_)
    if filter_type == "sos":
        print("\nbqfilter_coef = \n{", end="")
        for i in range(param.shape[0]):
            print("{", end="")
            for j in range(param.shape[1]):
                if j != param.shape[1] - 1:
                    print(param[i][j], end=", ")
                else:
                    print(param[i][j], end="")

            if i != param.shape[0] - 1:
                print("},")
            else:
                print("}", end="")
        print("};")

    if filter_type == "iir":
        print("\niir_filter_coef = \n{", end="")
        for i in range(param.shape[0]):
            print("{", end="")
            for j in range(param.shape[1]):
                if j != param.shape[1] - 1:
                    print(param[i][j], end=", ")
                else:
                    print(param[i][j], end="")

            if i != param.shape[0] - 1:
                print("},")
            else:
                print("}", end="")
        print("};")

    if filter_type == "fir":
        print("\nfir_filter_coef = \n{", end="")
        for i in range(param.shape[0]):
            if i != param.shape[0] - 1:
                print(param[i], end=", ")
            else:
                print(param[i], end="")
        print("};")


wp1 = WP
ws1 = WS
gpass1 = GPASS
gstop1 = GSTOP
ftype = FTYPE
fs = FS * UPSAMPLING_RATIO

sos = signal.iirdesign(wp1, ws1, gpass1, gstop1, output="sos", ftype=ftype, fs=fs)

inpulse_t = np.arange(0, TIME_IMPULSE_SIMU, 1 / FS)
impulse_y = signal.unit_impulse(inpulse_t.shape)
impulse_y_mormalized = impulse_y / impulse_y.max()

interp_func = interpolate.interp1d(
    inpulse_t, impulse_y_mormalized, kind="previous", fill_value="extrapolate"
)
impulse_t_interp = np.arange(0, TIME_IMPULSE_SIMU, 1 / FS / UPSAMPLING_RATIO)
impulse_y_interp = interp_func(impulse_t_interp)
indata = np.array([impulse_t_interp, impulse_y_interp])

x = indata[1]
y = signal.sosfilt(sos, x)

w, h = signal.sosfreqz(sos, fs=FS * UPSAMPLING_RATIO)

amp_dB = 20 * np.log10(np.abs(h))
angles = np.degrees(np.unwrap(np.angle(h)))
freq = w

fig1 = plt.figure()

ax1 = fig1.add_subplot(111)

ax1.plot(indata[0], y, label="result")

ax1.grid(True, "major", linestyle="-", linewidth=0.7)
ax1.grid(True, "minor", "x", linestyle="-", linewidth=0.3)

print_filter_params(sos, "sos")

fig2 = plt.figure()

ax2 = fig2.add_subplot(2, 1, 1)
ax2.plot(freq, amp_dB, "b")

ax2.grid(True, "major", linestyle="-", linewidth=0.7)
ax2.grid(True, "minor", "x", linestyle="-", linewidth=0.3)

ax2.set_xscale("log")

ax3 = fig2.add_subplot(2, 1, 2)
ax3.plot(freq, angles, "r")

ax3.set_xscale("log")

ax3.grid(True, "major", linestyle="-", linewidth=0.7)
ax3.grid(True, "minor", "x", linestyle="-", linewidth=0.3)

plt.show()
