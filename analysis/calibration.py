"""Measured refractive index against accepted values, with the best-fit line.

Writes calibration.png. The five (accepted, measured) pairs are the means from
Table 1 of the paper; see data/measurements.xlsx for the underlying trials."""
import numpy as np, matplotlib
matplotlib.use("Agg"); import matplotlib.pyplot as plt
INK,MUTED,BLUE="#22262B","#6B7280","#2E5FA3"
acc=np.array([1.3330,1.3600,1.3477,1.3505,1.4700])
mea=np.array([1.330,1.361,1.350,1.352,1.465])
lab=["Water","Glycerol 20%","Glucose 10%","NaCl 10%","Olive oil"]
off=[(-2,-13),(-26,2),(4,-13),(-6,9),(-2,-14)]
sl,ic=np.polyfit(acc,mea,1); r2=np.corrcoef(acc,mea)[0,1]**2
plt.rcParams.update({"font.family":"Liberation Serif","font.size":9,
  "axes.edgecolor":"#B9BEC5","axes.labelcolor":INK,"xtick.color":MUTED,
  "ytick.color":MUTED,"text.color":INK})
fig,ax=plt.subplots(figsize=(5.2,4.3),dpi=300)
x=np.linspace(1.325,1.475,50)
ax.plot(x,x,ls=(0,(5,3)),lw=1.0,color=MUTED,label="Ideal ($y=x$)")
ax.plot(x,sl*x+ic,lw=1.5,color=INK,
        label=f"Best fit: $y = {sl:.3f}x + {ic:.3f}$\n$R^2 = {r2:.4f}$")
ax.plot(acc,mea,"o",ms=6.5,mfc="white",mec=BLUE,mew=1.7,label="Measured",zorder=5)
for a,m,l,(dx,dy) in zip(acc,mea,lab,off):
    ax.annotate(l,(a,m),textcoords="offset points",xytext=(dx,dy),
                fontsize=7.6,color=MUTED,ha="center")
ax.set_xlabel("Accepted refractive index"); ax.set_ylabel("Refractive index measured by device")
ax.grid(True,color="#EDEFF2",lw=0.7); ax.set_axisbelow(True)
for s in ("top","right"): ax.spines[s].set_visible(False)
ax.legend(frameon=False,fontsize=8,loc="upper left")
fig.tight_layout(pad=0.3); fig.savefig("calibration.png",dpi=300,facecolor="white")
print(f"y = {sl:.4f}x + {ic:.4f}   R2 = {r2:.5f}")
