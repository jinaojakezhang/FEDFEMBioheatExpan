# Towards real-time finite-strain anisotropic thermo-visco-elastodynamic analysis of soft tissues for thermal ablative therapy (MIT License)
![GitHub](https://img.shields.io/github/license/jinaojakezhang/FEDFEMBioheatExpan)
![GitHub top language](https://img.shields.io/github/languages/top/jinaojakezhang/FEDFEMBioheatExpan)
<p align="center"><img src="https://user-images.githubusercontent.com/93865598/147813613-65d309fe-5f16-45ef-abaa-1f720c446608.PNG"></p>
This is the source repository for the paper:

| Zhang, J., Lay, R. J., Roberts, S. K., & Chauhan, S. (2021). Towards real-time finite-strain anisotropic thermo-visco-elastodynamic analysis of soft tissues for thermal ablative therapy. *Computer Methods and Programs in Biomedicine*, 198, 105789. [doi:10.1016/j.cmpb.2020.105789](https://www.sciencedirect.com/science/article/abs/pii/S0169260720316229). |
| --- |

which is based on the works of
| <p align="left">[1] Zhang, J., & Chauhan, S. (2019). Fast explicit dynamics finite element algorithm for transient heat transfer. *International Journal of Thermal Sciences*, 139, 160-175. [doi:10.1016/j.ijthermalsci.2019.01.030](https://www.sciencedirect.com/science/article/abs/pii/S1290072918317186) [[Code]](https://github.com/jinaojakezhang/FEDFEM).</p> |
| --- |
| <p align="left">**[2] Zhang, J., & Chauhan, S. (2019). Real-time computation of bio-heat transfer in the fast explicit dynamics finite element algorithm (FED-FEM) framework. *Numerical Heat Transfer, Part B: Fundamentals*, 75(4), 217-238. [doi:10.1080/10407790.2019.1627812](https://www.tandfonline.com/doi/abs/10.1080/10407790.2019.1627812) [[Code]](https://github.com/jinaojakezhang/FEDFEMBioheat).**</p> |
| <p align="left">**[3] Zhang, J., & Chauhan, S. (2020). Fast computation of soft tissue thermal response under deformation based on fast explicit dynamics finite element algorithm for surgical simulation. *Computer Methods and Programs in Biomedicine*, 187, 105244. [doi:10.1016/j.cmpb.2019.105244](https://www.sciencedirect.com/science/article/abs/pii/S0169260719311344) [[Code]](https://github.com/jinaojakezhang/FEDFEMBioheatDeform).**</p> |

Please cite the above paper if you use this code for your research.

If this code is helpful in your projects, please help to :star: this repo or recommend it to your friends. Thanks:blush:
## Environment:
- Windows 10
- Visual Studio 2017
-	OpenMP
## How to build:
1.	Download the source repository.
2.	Visual Studio 2017->Create New Project (Empty Project)->Project->Add Existing Item->BioheatExpan.cpp.
3.	Project->Properties->C/C++->Language->OpenMP Support->**Yes (/openmp)**.
4.	Build Solution (Release/x64).
## How to use:
1.	(cmd)Command Prompt->build path>project_name.exe input.txt. Example: <p align="center"><img src="https://user-images.githubusercontent.com/93865598/154496234-d17d1bc6-104e-4f85-a8d8-7d1df891283d.PNG"></p>
2.	Output: T.vtk, U.vtk, and Undeformed.vtk
## How to visualize:
1.	Open T.vtk and U.vtk. (such as using ParaView)
<p align="center"><img src="https://user-images.githubusercontent.com/93865598/154498494-fee77c78-531c-45f0-9bdc-f3bc016da88c.PNG"></p>

## How to make input.txt:
1.	Liver_Iso.inp (Abaqus input) is provided in the “models”, which was used to create Liver_Iso_n1.txt.
## Material types:
1.	Isotropic, orthotropic, and anisotropic thermal conductivities.
2.	Isotropic, transversely isotropic, and orthotropic thermal expansions.
3.	Neo-Hookean and Transversely Isotropic hyperelastic materials.
## Boundary conditions (BCs):
1.	Node index: Disp, FixP, HFlux, FixT.
2.	Element index: Perfu, BodyHFlux.
3.	All Elements: Gravity, Metabo.
## Notes:
1.	Node and Element index can start at 0, 1, or any but must be consistent in a file.
2.	Index starts at 0: *.txt.
3.	Index starts at 1: *_n1.txt.
## Feedback:
Please send an email to jinao.zhang@hotmail.com. Thanks for your valuable feedback and suggestions.
