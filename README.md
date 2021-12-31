# Towards real-time finite-strain anisotropic thermo-visco-elastodynamic analysis of soft tissues for thermal ablative therapy (MIT License)
[![download](https://img.shields.io/github/downloads/jinaojakezhang/FEDFEMBioheatExpan/total.svg)](https://github.com/jinaojakezhang/FEDFEMBioheatExpan/releases)
[![LICENSE](https://img.shields.io/github/license/jinaojakezhang/FEDFEMBioheatExpan.svg)](https://github.com/jinaojakezhang/FEDFEMBioheatExpan/blob/master/LICENSE)
![fig1](https://user-images.githubusercontent.com/93865598/147813613-65d309fe-5f16-45ef-abaa-1f720c446608.PNG)

This is the source repository for the paper:

Zhang, J., Lay, R. J., Roberts, S. K., & Chauhan, S. (2021). Towards real-time finite-strain anisotropic thermo-visco-elastodynamic analysis of soft tissues for thermal ablative therapy. Computer Methods and Programs in Biomedicine, 198, 105789. [doi:10.1016/j.cmpb.2020.105789](https://www.sciencedirect.com/science/article/abs/pii/S0169260720316229).

which is based on the works of [doi:10.1016/j.ijthermalsci.2019.01.030](https://www.sciencedirect.com/science/article/abs/pii/S1290072918317186) [[Code]](https://github.com/jinaojakezhang/FEDFEM), [doi:10.1080/10407790.2019.1627812](https://www.tandfonline.com/doi/abs/10.1080/10407790.2019.1627812) [[Code]](https://github.com/jinaojakezhang/FEDFEMBioheat), and [doi:10.1016/j.cmpb.2019.105244](https://www.sciencedirect.com/science/article/abs/pii/S0169260719311344) [[code]](https://github.com/jinaojakezhang/FEDFEMBioheatDeform).

Please cite the above paper if you use this code for your research.

If this code is helpful in your projects, please help to :star: this repo or recommend it to your friends. Thanks:blush:
# Environment:
•	Windows 10

•	Visual Studio 2017

•	OpenMP
# How to build:
1.	Download the source repository.
2.	Visual Studio 2017->Create New Project (Empty Project)->Project->Add Existing Item->BioheatExpan.cpp.
3.	Project->Properties->C/C++->Language->OpenMP Support->Yes (/openmp).
4.	Build Solution (Release/x64).
# How to use:
1.	(cmd)Command Prompt-> ![fig2](https://user-images.githubusercontent.com/93865598/147813614-2cad8236-badf-4fb1-a828-8d85f5bc0c51.PNG)
2.	Output: T.vtk, U.vtk, and Undeformed.vtk
# How to visualize:
1.	Open T.vtk and U.vtk. (such as using ParaView)

![fig3](https://user-images.githubusercontent.com/93865598/147813617-431efd6a-2bec-4de2-8562-a3c7e4188476.PNG)
# Boundary condition (BC):
1.	Node index: Disp, FixP, HFlux, FixT.
2.	Element index: Perfu, BodyHFlux.
3.	All Elements: Gravity, Metabo.
# Notes:
1.	Node and Element index can start at 0, 1, or any but must be consistent in a file.
2.	Liver_Iso.txt, node and element index start at 0; Liver_Iso_n1.txt, node and element index start at 1.
# Feedback:
Please send an email to jinao.zhang@hotmail.com. Thanks for your valuable feedback and suggestions.
