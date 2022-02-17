/*
MIT License

Copyright (c) 2021 Jinao Zhang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <iostream>
#include <vector>
#include <chrono>
#include <fstream>
#include <omp.h>
using namespace    std;
static const int   NUM_THREADS(omp_get_max_threads());

// matrix computation/operations (mat: matrix, 33: 3 rows by 3 columns, x: multiplication, T: transpose, Det: determinant, Inv: inverse)
void mat33x33    (const float A[3][3], const float B[3][3], float AB[3][3]);
void mat33x34    (const float A[3][3], const float B[3][4], float AB[3][4]);
void mat33Tx33   (const float A[3][3], const float B[3][3], float AB[3][3]);
void mat33Tx34   (const float A[3][3], const float B[3][4], float AB[3][4]);
void mat34Tx34   (const float A[3][4], const float B[3][4], float AB[4][4]);
void mat33x33T   (const float A[3][3], const float B[3][3], float AB[3][3]);
void mat34x34T   (const float A[3][4], const float B[3][4], float AB[3][3]);
void mat33xScalar(const float A[3][3], const float b,       float Ab[3][3]);
void mat44xScalar(const float A[4][4], const float b,       float Ab[4][4]);
void matDet33    (const float A[3][3], float &detA);
void matInv33    (const float A[3][3], float invA[3][3], float &detA);

// classes
class Node;
class T4;
class Model;
class ModelStates;

// methods
Model*       readModel       (int argc, char **argv);
void         printInfo       (const Model& model);
ModelStates* runSimulation   (const Model& model);
void         initBC          (const Model& model, ModelStates& modelstates);
void         computeRunTimeBC(const Model& model, ModelStates& modelstates, const size_t curr_step);
bool         computeOneStep  (const Model& model, ModelStates& modelstates);
int          exportVTK       (const Model& model, const ModelStates& modelstates);

class Node
{
public:
    const unsigned int m_idx;
    const float        m_x, m_y, m_z;
    Node(const unsigned int idx, const float x, const float y, const float z) :
        m_idx(idx), m_x(x), m_y(y), m_z(z) {};
};

class T4
{
public:
    const unsigned int m_idx, m_n_idx[4];
    const float        m_DHDr[3][4];
    float              m_DHDX[3][4], m_DHDx[3][4],
                       m_S[3][3], m_X[3][3], m_X_expan[3][3], // S: 2nd PK stress; X: defor.grad; X_expan: thermal expansion defor.grad
                       m_D[3][3], m_K[4][4],                  // D: conductivity; K: conduction
                       m_Vol, m_vol, m_mass;
    const string       m_M_material_type, m_T_material_type, m_T_expan_type;
    vector<float>      m_M_material_vals, m_T_material_vals, m_T_expan_vals;
    T4(const unsigned int idx, const Node& n1, const Node& n2, const Node& n3, const Node& n4, const float rho, const string M_material_type, const vector<float>& M_material_vals, const string T_material_type, const vector<float>& T_material_vals, const string T_expan_type, const vector<float>& T_expan_vals) :
        m_idx(idx), m_n_idx{ n1.m_idx, n2.m_idx, n3.m_idx, n4.m_idx }, m_M_material_type(M_material_type), m_M_material_vals(M_material_vals.cbegin(), M_material_vals.cend()), m_T_material_type(T_material_type), m_T_material_vals(T_material_vals.cbegin(), T_material_vals.cend()), m_T_expan_type(T_expan_type), m_T_expan_vals(T_expan_vals.cbegin(), T_expan_vals.cend()),
        m_DHDr{{-1, 1, 0, 0},
               {-1, 0, 1, 0},
               {-1, 0, 0, 1}}
    {
        float n_coords[3][4], J0[3][3], detJ0(0.f), invJ0[3][3];
        n_coords[0][0] = n1.m_x; n_coords[1][0] = n1.m_y; n_coords[2][0] = n1.m_z;
        n_coords[0][1] = n2.m_x; n_coords[1][1] = n2.m_y; n_coords[2][1] = n2.m_z;
        n_coords[0][2] = n3.m_x; n_coords[1][2] = n3.m_y; n_coords[2][2] = n3.m_z;
        n_coords[0][3] = n4.m_x; n_coords[1][3] = n4.m_y; n_coords[2][3] = n4.m_z;
        mat34x34T(m_DHDr, n_coords, J0);
        matInv33(J0, invJ0, detJ0);
        m_Vol = detJ0 / 6.f;
        m_vol = m_Vol;
        m_mass = rho * m_Vol;
        mat33x34(invJ0, m_DHDr, m_DHDX);
        memset(m_DHDx, 0, sizeof(float) * 3 * 4);
        memset(m_S, 0, sizeof(float) * 3 * 3); memset(m_X, 0, sizeof(float) * 3 * 3); memset(m_X_expan, 0, sizeof(float) * 3 * 3);
        if (m_T_material_type == "T_ISO") // [0]=c, [1]=k
        {
            memset(m_D, 0, sizeof(float) * 3 * 3);
            m_D[0][0] = m_T_material_vals[1]; m_D[1][1] = m_T_material_vals[1]; m_D[2][2] = m_T_material_vals[1];
        }
        else if (m_T_material_type == "T_ORTHO") // [0]=c, [1]=k11, [2]=k22, [3]=k33
        {
            memset(m_D, 0, sizeof(float) * 3 * 3);
            m_D[0][0] = m_T_material_vals[1]; m_D[1][1] = m_T_material_vals[2]; m_D[2][2] = m_T_material_vals[3];
        }
        else if (m_T_material_type == "T_ANISO") // [0]=c, [1]=k11, [2]=k12, [3]=k13, [4]=k22, [5]=k23, [6]=k33
        {
            memset(m_D, 0, sizeof(float) * 3 * 3);
            m_D[0][0] = m_T_material_vals[1]; m_D[0][1] = m_T_material_vals[2]; m_D[0][2] = m_T_material_vals[3];
            m_D[1][0] = m_D[0][1];            m_D[1][1] = m_T_material_vals[4]; m_D[1][2] = m_T_material_vals[5];
            m_D[2][0] = m_D[0][2];            m_D[2][1] = m_D[1][2];            m_D[2][2] = m_T_material_vals[6];
        }
        float temp[3][4];
        mat33x34(m_D, m_DHDX, temp);
        mat34Tx34(m_DHDX, temp, m_K);
        mat44xScalar(m_K, m_Vol, m_K);
    };
};

class Model
{
public:
    vector<Node*>        m_nodes;
    vector<T4*>          m_tets;
    size_t               m_num_BCs,    m_num_steps,  m_num_M_DOFs, m_num_T_DOFs;
    vector<unsigned int> m_disp_idx_x, m_disp_idx_y, m_disp_idx_z,
                         m_fixP_idx_x, m_fixP_idx_y, m_fixP_idx_z,
                         m_hflux_idx,  m_perfu_idx,  m_fixT_idx,   m_bhflux_idx;
    vector<float>        m_disp_mag_x, m_disp_mag_y, m_disp_mag_z,
                         m_grav_f_x,   m_grav_f_y,   m_grav_f_z,
                         m_hflux_mag,
                         m_perfu_refT, m_perfu_const1,
                         m_fixT_mag,
                         m_bhflux_mag,
                         m_metabo_mag,
                         m_M_material_vals, m_T_material_vals, m_T_expan_vals;
    float                m_dt, m_total_t, m_alpha, m_T0, m_rho;
    const string         m_fname;
    string               m_ele_type, m_M_material_type, m_T_material_type, m_T_expan_type;
    unsigned int         m_node_begin_index, m_ele_begin_index,
                        *m_ele_node_local_idx_pair,
                        *m_tracking_num_eles_i_eles_per_node_j;
    Model(const string fname) :
        m_nodes     (0), m_tets      (0),
        m_num_BCs   (0), m_num_steps (0), m_num_M_DOFs  (0), m_num_T_DOFs(0),
        m_disp_idx_x(0), m_disp_idx_y(0), m_disp_idx_z  (0),
        m_fixP_idx_x(0), m_fixP_idx_y(0), m_fixP_idx_z  (0),
        m_disp_mag_x(0), m_disp_mag_y(0), m_disp_mag_z  (0),
        m_grav_f_x  (0), m_grav_f_y  (0), m_grav_f_z    (0),
        m_hflux_idx (0), m_hflux_mag (0),
        m_perfu_idx (0), m_perfu_refT(0), m_perfu_const1(0),
        m_fixT_idx  (0), m_fixT_mag  (0),
        m_bhflux_idx(0), m_bhflux_mag(0),
        m_metabo_mag(0),
        m_M_material_vals(0), m_T_material_vals(0), m_T_expan_vals(0),
        m_dt(0.f), m_total_t(0.f), m_alpha(0.f), m_T0(0.f), m_rho(0.f),
        m_fname(fname), m_ele_type(""), m_M_material_type(""), m_T_material_type(""), m_T_expan_type(""),
        m_node_begin_index(0), m_ele_begin_index(0),
        m_ele_node_local_idx_pair(nullptr), m_tracking_num_eles_i_eles_per_node_j(nullptr) {};
    ~Model()
    {
        for (Node* node : m_nodes) { delete node; }
        for (T4* tet : m_tets)     { delete tet; }
        delete[] m_ele_node_local_idx_pair;
        delete[] m_tracking_num_eles_i_eles_per_node_j;
    };
    void postCreate()
    {
        // below: provide indexing for nodal states (e.g., individual ele nodal internal forces and thermal loads) to avoid race condition in parallel computing
        m_tracking_num_eles_i_eles_per_node_j = new unsigned int[m_nodes.size() * 2]; memset(m_tracking_num_eles_i_eles_per_node_j, 0, sizeof(unsigned int) * m_nodes.size() * 2);
        vector<vector<unsigned int>> nodes_ele_node_local_idx_pair(m_nodes.size());
        for (T4* tet : m_tets) { for (unsigned int m = 0; m < 4; m++) { nodes_ele_node_local_idx_pair[tet->m_n_idx[m]].push_back(tet->m_idx); nodes_ele_node_local_idx_pair[tet->m_n_idx[m]].push_back(m); } }
        vector<unsigned int> eles_per_node(m_nodes.size(), 0);
        unsigned int length(0);
        for (size_t m = 0; m < m_nodes.size(); m++)
        {
            eles_per_node[m] = (unsigned int)nodes_ele_node_local_idx_pair[m].size() / 2;
            length += eles_per_node[m];
        }
        m_ele_node_local_idx_pair = new unsigned int[length * 2];
        unsigned int* p_ele_node_local_idx_pair = m_ele_node_local_idx_pair, tracking(0);
        for (size_t m = 0; m < m_nodes.size(); m++)
        {
            m_tracking_num_eles_i_eles_per_node_j[m * 2 + 0] = tracking;
            m_tracking_num_eles_i_eles_per_node_j[m * 2 + 1] = eles_per_node[m]; tracking += eles_per_node[m];
            for (size_t n = 0; n < eles_per_node[m]; n++)
            {
                *p_ele_node_local_idx_pair = nodes_ele_node_local_idx_pair[m][n * 2 + 0]; p_ele_node_local_idx_pair++; // tet->m_idx
                *p_ele_node_local_idx_pair = nodes_ele_node_local_idx_pair[m][n * 2 + 1]; p_ele_node_local_idx_pair++; // m
            }
        }
    }
};

class ModelStates
{
public:
    vector<float> m_external_F,          m_ele_nodal_internal_F,                        // individual ele nodal internal F to avoid race condition, can be summed to get internal_F for nodes
                  m_disp_mag_t,
                  m_central_diff_const1, m_central_diff_const2, m_central_diff_const3,
                  m_prev_U,              m_curr_U,              m_next_U,
                  m_external_Q,          m_external_Q0,         m_ele_nodal_internal_Q, // individual ele nodal internal Q to avoid race condition, can be summed to get internal_Q for nodes
                  m_fixT_mag,
                  m_constA,
                  m_curr_T,              m_next_T;
    vector<bool>  m_fixP_flag,           m_fixT_flag;
    ModelStates(const Model& model) :
        m_external_F         (model.m_num_M_DOFs,        0.f), m_ele_nodal_internal_F(model.m_tets.size() * 4 * 3, 0.f),
        m_disp_mag_t         (model.m_num_M_DOFs,        0.f),
        m_central_diff_const1(model.m_num_M_DOFs,        0.f), m_central_diff_const2 (model.m_num_M_DOFs,          0.f), m_central_diff_const3 (model.m_num_M_DOFs,      0.f),
        m_prev_U             (model.m_num_M_DOFs,        0.f), m_curr_U              (model.m_num_M_DOFs,          0.f), m_next_U              (model.m_num_M_DOFs,      0.f),
        m_external_Q         (model.m_num_T_DOFs,        0.f), m_external_Q0         (model.m_num_T_DOFs,          0.f), m_ele_nodal_internal_Q(model.m_tets.size() * 4, 0.f),
        m_fixT_mag           (model.m_num_T_DOFs,        0.f),
        m_constA             (model.m_num_T_DOFs,        0.f),
        m_curr_T             (model.m_num_T_DOFs, model.m_T0), m_next_T              (model.m_num_T_DOFs,   model.m_T0),
        m_fixP_flag          (model.m_num_M_DOFs,      false), m_fixT_flag           (model.m_num_T_DOFs,        false)
    {
        vector<float> nodal_M_mass(model.m_num_M_DOFs, 0.f);
        for (T4* tet : model.m_tets) { for (size_t m = 0; m < 4; m++) { for (size_t n = 0; n < 3; n++) { nodal_M_mass[tet->m_n_idx[m] * 3 + n] += tet->m_mass / 4.f; } } }
        for (size_t i = 0; i < model.m_num_M_DOFs; i++)
        {
            m_central_diff_const1[i] = 1.f / (model.m_alpha * nodal_M_mass[i] / 2.f / model.m_dt + nodal_M_mass[i] / model.m_dt / model.m_dt);
            m_central_diff_const2[i] = 2.f * nodal_M_mass[i] * m_central_diff_const1[i] / model.m_dt / model.m_dt;
            m_central_diff_const3[i] = model.m_alpha * nodal_M_mass[i] * m_central_diff_const1[i] / 2.f / model.m_dt - m_central_diff_const2[i] / 2.f;
        }
        vector<float> nodal_T_mass(model.m_num_T_DOFs, 0.f);
        for (T4* tet : model.m_tets) { for (size_t m = 0; m < 4; m++) { nodal_T_mass[tet->m_n_idx[m]] += tet->m_mass / 4.f; } }
        for (size_t i = 0; i < model.m_num_T_DOFs; i++) { m_constA[i] = model.m_dt / (nodal_T_mass[i] * model.m_T_material_vals[0]); }
    };
};

int main(int argc, char **argv)
{
    Model* model = readModel(argc, argv);
    if (model != nullptr)
    {
        printInfo(*model);
        ModelStates* modelstates = runSimulation(*model);
        if (modelstates != nullptr)
        {
            int exit = exportVTK(*model, *modelstates);
            delete model;
            delete modelstates;
            return exit;
        }
        else { return EXIT_FAILURE; }
    }
    else { return EXIT_FAILURE; }
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
Model* readModel(int argc, char **argv)
{
    if (argc - 1 == 0) { cerr << "\n\tError: missing input argument (e.g., Liver_Iso.txt)." << endl; return nullptr; }
    FILE* file;
    if (fopen_s(&file, argv[1], "r") != 0) { cerr << "\n\tError: cannot open file: " << argv[1] << endl; return nullptr; }
    else
    {
        Model* model = new Model(argv[1]);
        char buffer[256];
        unsigned int idx(0); float x(0.f), y(0.f), z(0.f);
        fscanf_s(file, "%u %f %f %f", &idx, &x, &y, &z); model->m_node_begin_index = idx; model->m_nodes.push_back(new Node(idx - model->m_node_begin_index, x, y, z)); // for first node only, to get node begin index
        while (fscanf_s(file, "%u %f %f %f", &idx, &x, &y, &z)) { model->m_nodes.push_back(new Node(idx - model->m_node_begin_index, x, y, z)); } // internally, node index starts at 0
        fscanf_s(file, "%s", buffer, (unsigned int)sizeof(buffer)); model->m_M_material_type = buffer;
        if (model->m_M_material_type == "NH")
        {
            float Mu(0.f), K(0.f); fscanf_s(file, "%f %f", &Mu, &K); model->m_M_material_vals.push_back(Mu); model->m_M_material_vals.push_back(K);
        }
        else if (model->m_M_material_type == "TI")
        {
            float Mu(0.f), K(0.f), Eta(0.f), a[3]; fscanf_s(file, "%f %f %f %f %f %f", &Mu, &K, &Eta, &a[0], &a[1], &a[2]); model->m_M_material_vals.push_back(Mu); model->m_M_material_vals.push_back(K);
            float mag = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); if (mag != 1.f) { a[0] /= mag; a[1] /= mag; a[2] /= mag; } // normalise
            float A00 = a[0] * a[0], A01 = a[0] * a[1], A02 = a[0] * a[2], A11 = a[1] * a[1], A12 = a[1] * a[2], A22 = a[2] * a[2];
            model->m_M_material_vals.push_back(Eta); model->m_M_material_vals.push_back(A00); model->m_M_material_vals.push_back(A01); model->m_M_material_vals.push_back(A02); model->m_M_material_vals.push_back(A11); model->m_M_material_vals.push_back(A12); model->m_M_material_vals.push_back(A22);
        }
        else if (model->m_M_material_type == "other_material_types") { /*add your code here*/ }
        fscanf_s(file, "%s", buffer, (unsigned int)sizeof(buffer)); model->m_T_material_type = buffer;
        if      (model->m_T_material_type == "T_ISO")   { float c(0.f), k(0.f); fscanf_s(file, "%f %f", &c, &k); model->m_T_material_vals.push_back(c); model->m_T_material_vals.push_back(k); }
        else if (model->m_T_material_type == "T_ORTHO") { float c(0.f), k11(0.f), k22(0.f), k33(0.f); fscanf_s(file, "%f %f %f %f", &c, &k11, &k22, &k33); model->m_T_material_vals.push_back(c); model->m_T_material_vals.push_back(k11); model->m_T_material_vals.push_back(k22); model->m_T_material_vals.push_back(k33); }
        else if (model->m_T_material_type == "T_ANISO") { float c(0.f), k11(0.f), k12(0.f), k13(0.f), k22(0.f), k23(0.f), k33(0.f); fscanf_s(file, "%f %f %f %f %f %f %f", &c, &k11, &k12, &k13, &k22, &k23, &k33); model->m_T_material_vals.push_back(c); model->m_T_material_vals.push_back(k11); model->m_T_material_vals.push_back(k12); model->m_T_material_vals.push_back(k13); model->m_T_material_vals.push_back(k22); model->m_T_material_vals.push_back(k23); model->m_T_material_vals.push_back(k33); }
        else if (model->m_T_material_type == "other_conductivity_types") { /*add your code here*/ }
        fscanf_s(file, "%s", buffer, (unsigned int)sizeof(buffer)); model->m_T_expan_type = buffer;
        if (model->m_T_expan_type == "T_EXPAN_ISO")
        {
            float alpha_i(0.f); fscanf_s(file, "%f", &alpha_i); model->m_T_expan_vals.push_back(alpha_i);
        }
        else if (model->m_T_expan_type == "T_EXPAN_TI")
        {
            float alpha_i(0.f), alpha_m(0.f), m[3]; fscanf_s(file, "%f %f %f %f %f", &alpha_i, &alpha_m, &m[0], &m[1], &m[2]); model->m_T_expan_vals.push_back(alpha_i);
            const float mag = sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]); if (mag != 1.f) { m[0] /= mag; m[1] /= mag; m[2] /= mag; } // normalise
            const float M00 = m[0] * m[0], M01 = m[0] * m[1], M02 = m[0] * m[2], M11 = m[1] * m[1], M12 = m[1] * m[2], M22 = m[2] * m[2];
            model->m_T_expan_vals.push_back(alpha_m - alpha_i); model->m_T_expan_vals.push_back(M00); model->m_T_expan_vals.push_back(M01); model->m_T_expan_vals.push_back(M02); model->m_T_expan_vals.push_back(M11); model->m_T_expan_vals.push_back(M12); model->m_T_expan_vals.push_back(M22);
        }
        else if (model->m_T_expan_type == "T_EXPAN_ORTHO")
        {
            float alpha_i(0.f), alpha_m(0.f), m[3], alpha_n(0.f), n[3]; fscanf_s(file, "%f %f %f %f %f %f %f %f %f", &alpha_i, &alpha_m, &m[0], &m[1], &m[2], &alpha_n, &n[0], &n[1], &n[2]); model->m_T_expan_vals.push_back(alpha_i);
            const float magm = sqrt(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]); if (magm != 1.f) { m[0] /= magm; m[1] /= magm; m[2] /= magm; } // normalise
            const float M00 = m[0] * m[0], M01 = m[0] * m[1], M02 = m[0] * m[2], M11 = m[1] * m[1], M12 = m[1] * m[2], M22 = m[2] * m[2];
            model->m_T_expan_vals.push_back(alpha_m - alpha_i); model->m_T_expan_vals.push_back(M00); model->m_T_expan_vals.push_back(M01); model->m_T_expan_vals.push_back(M02); model->m_T_expan_vals.push_back(M11); model->m_T_expan_vals.push_back(M12); model->m_T_expan_vals.push_back(M22);
            const float magn = sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]); if (magn != 1.f) { n[0] /= magn; n[1] /= magn; n[2] /= magn; } // normalise
            const float N00 = n[0] * n[0], N01 = n[0] * n[1], N02 = n[0] * n[2], N11 = n[1] * n[1], N12 = n[1] * n[2], N22 = n[2] * n[2];
            model->m_T_expan_vals.push_back(alpha_n - alpha_i); model->m_T_expan_vals.push_back(N00); model->m_T_expan_vals.push_back(N01); model->m_T_expan_vals.push_back(N02); model->m_T_expan_vals.push_back(N11); model->m_T_expan_vals.push_back(N12); model->m_T_expan_vals.push_back(N22);
        }
        else if (model->m_T_expan_type == "other_expansion_types") { /*add your code here*/ }
        fscanf_s(file, "%s %f", buffer, (unsigned int)sizeof(buffer), &model->m_rho);
        fscanf_s(file, "%s", buffer, (unsigned int)sizeof(buffer)); model->m_ele_type = buffer;
        unsigned int n1_idx(0), n2_idx(0), n3_idx(0), n4_idx(0);
        fscanf_s(file, "%u %u %u %u %u", &idx, &n1_idx, &n2_idx, &n3_idx, &n4_idx); model->m_ele_begin_index = idx; model->m_tets.push_back(new T4(idx - model->m_ele_begin_index,
                                                                                                                                                   *model->m_nodes[n1_idx - model->m_node_begin_index], *model->m_nodes[n2_idx - model->m_node_begin_index], *model->m_nodes[n3_idx - model->m_node_begin_index], *model->m_nodes[n4_idx - model->m_node_begin_index],
                                                                                                                                                    model->m_rho, model->m_M_material_type, model->m_M_material_vals, model->m_T_material_type, model->m_T_material_vals, model->m_T_expan_type, model->m_T_expan_vals)); // for first ele only, to get ele begin index
        while (fscanf_s(file, "%u %u %u %u %u", &idx, &n1_idx, &n2_idx, &n3_idx, &n4_idx)) { model->m_tets.push_back(new T4(idx - model->m_ele_begin_index,
                                                                                                                            *model->m_nodes[n1_idx - model->m_node_begin_index], *model->m_nodes[n2_idx - model->m_node_begin_index], *model->m_nodes[n3_idx - model->m_node_begin_index], *model->m_nodes[n4_idx - model->m_node_begin_index],
                                                                                                                             model->m_rho, model->m_M_material_type, model->m_M_material_vals, model->m_T_material_type, model->m_T_material_vals, model->m_T_expan_type, model->m_T_expan_vals)); } // internally, ele index starts at 0
        while (fscanf_s(file, "%s", buffer, (unsigned int)sizeof(buffer)))
        {
            string BC_type(buffer), xyz("");
            if (BC_type == "<Disp>") // Displacements
            {
                float u(0.f); fscanf_s(file, "%s %f", buffer, (unsigned int)sizeof(buffer), &u); xyz = buffer;
                if      (xyz == "x") { while (fscanf_s(file, "%u", &idx)) { model->m_disp_idx_x.push_back(idx - model->m_node_begin_index); model->m_disp_mag_x.push_back(u); } }
                else if (xyz == "y") { while (fscanf_s(file, "%u", &idx)) { model->m_disp_idx_y.push_back(idx - model->m_node_begin_index); model->m_disp_mag_y.push_back(u); } }
                else if (xyz == "z") { while (fscanf_s(file, "%u", &idx)) { model->m_disp_idx_z.push_back(idx - model->m_node_begin_index); model->m_disp_mag_z.push_back(u); } }
                model->m_num_BCs++;
            }
            else if (BC_type == "<FixP>") // Fixed positions
            {
                fscanf_s(file, "%s", buffer, (unsigned int)sizeof(buffer)); xyz = buffer;
                if      (xyz == "x")   { while (fscanf_s(file, "%u", &idx)) { model->m_fixP_idx_x.push_back(idx - model->m_node_begin_index); } }
                else if (xyz == "y")   { while (fscanf_s(file, "%u", &idx)) { model->m_fixP_idx_y.push_back(idx - model->m_node_begin_index); } }
                else if (xyz == "z")   { while (fscanf_s(file, "%u", &idx)) { model->m_fixP_idx_z.push_back(idx - model->m_node_begin_index); } }
                else if (xyz == "all") { while (fscanf_s(file, "%u", &idx)) { model->m_fixP_idx_x.push_back(idx - model->m_node_begin_index); model->m_fixP_idx_y.push_back(idx - model->m_node_begin_index); model->m_fixP_idx_z.push_back(idx - model->m_node_begin_index); } }
                model->m_num_BCs++;
            }
            else if (BC_type == "<Gravity>") // Gravity
            {
                float g(0.f); fscanf_s(file, "%s %f", buffer, (unsigned int)sizeof(buffer), &g); xyz = buffer;
                if      (xyz == "x") { model->m_grav_f_x.resize(model->m_nodes.size(), 0.f); for (T4* tet : model->m_tets) { for (size_t m = 0; m < 4; m++) { model->m_grav_f_x[tet->m_n_idx[m]] += tet->m_mass * g / 4.f; } } }
                else if (xyz == "y") { model->m_grav_f_y.resize(model->m_nodes.size(), 0.f); for (T4* tet : model->m_tets) { for (size_t m = 0; m < 4; m++) { model->m_grav_f_y[tet->m_n_idx[m]] += tet->m_mass * g / 4.f; } } }
                else if (xyz == "z") { model->m_grav_f_z.resize(model->m_nodes.size(), 0.f); for (T4* tet : model->m_tets) { for (size_t m = 0; m < 4; m++) { model->m_grav_f_z[tet->m_n_idx[m]] += tet->m_mass * g / 4.f; } } }
                model->m_num_BCs++;
            }
            else if (BC_type == "<HFlux>") // Nodal heat flux
            {
                float q(0.f); fscanf_s(file, "%f", &q);
                while (fscanf_s(file, "%u", &idx)) { model->m_hflux_idx.push_back(idx - model->m_node_begin_index); model->m_hflux_mag.push_back(q); }
                model->m_num_BCs++;
            }
            else if (BC_type == "<Perfu>") // Perfusion
            {
                float wb(0.f), cb(0.f), refT(0.f); fscanf_s(file, "%f %f %f", &wb, &cb, &refT);
                vector<float> nodal_wbVolcb(model->m_nodes.size(), 0.f);
                while (fscanf_s(file, "%u", &idx)) { for (size_t m = 0; m < 4; m++) { nodal_wbVolcb[model->m_tets[idx - model->m_ele_begin_index]->m_n_idx[m]] += wb * model->m_tets[idx - model->m_ele_begin_index]->m_Vol / 4.f * cb; } }
                for (unsigned int i = 0; i < nodal_wbVolcb.size(); i++) { if (nodal_wbVolcb[i] != 0) { model->m_perfu_idx.push_back(i); model->m_perfu_const1.push_back(nodal_wbVolcb[i]); model->m_perfu_refT.push_back(refT); } }
                model->m_num_BCs++;
            }
            else if (BC_type == "<FixT>") // Fixed temperature
            {
                float constT(0.f); fscanf_s(file, "%f", &constT);
                while (fscanf_s(file, "%u", &idx)) { model->m_fixT_idx.push_back(idx - model->m_node_begin_index); model->m_fixT_mag.push_back(constT); }
                model->m_num_BCs++;
            }
            else if (BC_type == "<BodyHFlux>") // Body heat flux
            {
                float q(0.f); fscanf_s(file, "%f", &q);
                vector<float> nodal_q(model->m_nodes.size(), 0.f);
                while (fscanf_s(file, "%u", &idx)) { for (size_t m = 0; m < 4; m++) { nodal_q[model->m_tets[idx - model->m_ele_begin_index]->m_n_idx[m]] += q * model->m_tets[idx - model->m_ele_begin_index]->m_Vol / 4.f; } }
                for (unsigned int i = 0; i < nodal_q.size(); i++) { if (nodal_q[i] != 0) { model->m_bhflux_idx.push_back(i); model->m_bhflux_mag.push_back(nodal_q[i]); } }
                model->m_num_BCs++;
            }
            else if (BC_type == "<Metabo>") // Metabolic heat generation
            {
                float q(0.f); fscanf_s(file, "%f", &q);
                model->m_metabo_mag.resize(model->m_nodes.size(), 0.f); for (T4* tet : model->m_tets) { for (size_t m = 0; m < 4; m++) { model->m_metabo_mag[tet->m_n_idx[m]] += q * tet->m_Vol / 4.f; } }
                model->m_num_BCs++;
            }
            else if (BC_type == "other_BC_types") { /*add your code here*/ }
            else if (BC_type == "</BC>") { break; }
        }
        fscanf_s(file, "%s %f", buffer, (unsigned int)sizeof(buffer), &model->m_alpha);
        fscanf_s(file, "%s %f", buffer, (unsigned int)sizeof(buffer), &model->m_T0);
        fscanf_s(file, "%s %f", buffer, (unsigned int)sizeof(buffer), &model->m_dt);
        fscanf_s(file, "%s %f", buffer, (unsigned int)sizeof(buffer), &model->m_total_t);
        fclose(file);
        model->m_num_steps = (size_t)ceil(model->m_total_t / model->m_dt);
        model->m_num_M_DOFs = model->m_nodes.size() * 3;
        model->m_num_T_DOFs = model->m_nodes.size() * 1;
        model->postCreate();
        return model;
    }
}

void printInfo(const Model& model)
{
    cout << endl;
    cout << "\t---------------------------------------------------------------------------------------------------" << endl;
    cout << "\t| Oepn-source (OpenMP) implmentation of:                                                          |" << endl;
    cout << "\t|             <Towards real-time finite-strain anisotropic thermo-visco-elastodynamic analysis... |" << endl;
    cout << "\t|                                                   of soft tissues for thermal ablative therapy. |" << endl;
    cout << "\t|                                   Zhang, J., Lay, R. J., Roberts, S. K., & Chauhan, S. (2021).  |" << endl;
    cout << "\t|                                                   Comput Methods Programs Biomed, 198, 105789.  |" << endl;
    cout << "\t|                                                                 doi:10.1016/j.cmpb.2020.105789> |" << endl;
    cout << "\t|                                                                                  by Jinao Zhang |" << endl;
    cout << "\t---------------------------------------------------------------------------------------------------" << endl;
    cout << "\tModel:\t\t"      << model.m_fname.c_str()           << endl;
    cout << "\tNodes:\t\t"      << model.m_nodes.size()            << " (" << model.m_num_M_DOFs + model.m_num_T_DOFs << " DOFs)" << endl;
    cout << "\tElements:\t"     << model.m_tets.size()             << " (" << model.m_ele_type.c_str() << ")" << endl;
    cout << "\tEleMaterial:\t"  << model.m_M_material_type.c_str() << ":"; for (const float val : model.m_M_material_vals) { cout << " " << val; } cout << endl;
    cout << "\t\t\t"            << model.m_T_material_type.c_str() << ":"; for (const float val : model.m_T_material_vals) { cout << " " << val; } cout << endl;
    cout << "\t\t\t"            << model.m_T_expan_type.c_str()    << ":"; for (const float val : model.m_T_expan_vals)    { cout << " " << val; } cout << endl;
    cout << "\t\t\tDensity: "   << model.m_rho                     << endl;
    cout << "\tBC:\t\t"         << model.m_num_BCs                 << endl;
    cout << "\tDampingCoef.:\t" << model.m_alpha                   << endl;
    cout << "\tInitialTemp.:\t" << model.m_T0                      << endl;
    cout << "\tTimeStep:\t"     << model.m_dt                      << endl;
    cout << "\tTotalTime:\t"    << model.m_total_t                 << endl;
    cout << "\tNumSteps:\t"     << model.m_num_steps               << endl;
    cout << "\n\tNode index starts at " << model.m_node_begin_index << "." << endl;
    cout << "  \tElem index starts at " << model.m_ele_begin_index  << "." << endl;
}

ModelStates* runSimulation(const Model& model)
{
    ModelStates* modelstates = new ModelStates(model);
    initBC(model, *modelstates);
    size_t progress(0);
    auto start_t = chrono::high_resolution_clock::now();
    cout << "\n\tusing " << NUM_THREADS << " threads" << endl;
    cout << "\tcomputing..." << endl;
    for (size_t step = 0; step < model.m_num_steps; step++) // simulation loop
    {
        if ((float)(step + 1) / (float)model.m_num_steps * 100.f >= progress + 10) { progress += 10; cout << "\t\t\t(" << progress << "%)" << endl; }
        computeRunTimeBC(model, *modelstates, step);
        bool no_err = computeOneStep(model, *modelstates);
        if (!no_err) { delete modelstates; return nullptr; }
    }
    auto elapsed = chrono::high_resolution_clock::now() - start_t;
    long long t = chrono::duration_cast<chrono::milliseconds>(elapsed).count();
    cout << "\n\tComputation time:\t" << t << " ms" << endl;
    return modelstates;
}

void initBC(const Model& model, ModelStates& modelstates)
{
    fill(modelstates.m_external_F.begin(), modelstates.m_external_F.end(), 0.f);
    // BC:Gravity
    for (size_t i = 0; i < model.m_grav_f_x.size(); i++) { modelstates.m_external_F[i * 3 + 0] += model.m_grav_f_x[i]; }
    for (size_t i = 0; i < model.m_grav_f_y.size(); i++) { modelstates.m_external_F[i * 3 + 1] += model.m_grav_f_y[i]; }
    for (size_t i = 0; i < model.m_grav_f_z.size(); i++) { modelstates.m_external_F[i * 3 + 2] += model.m_grav_f_z[i]; }
    // BC:FixP
    for (size_t i = 0; i < model.m_fixP_idx_x.size(); i++) { modelstates.m_fixP_flag[model.m_fixP_idx_x[i] * 3 + 0] = true; }
    for (size_t i = 0; i < model.m_fixP_idx_y.size(); i++) { modelstates.m_fixP_flag[model.m_fixP_idx_y[i] * 3 + 1] = true; }
    for (size_t i = 0; i < model.m_fixP_idx_z.size(); i++) { modelstates.m_fixP_flag[model.m_fixP_idx_z[i] * 3 + 2] = true; }
    fill(modelstates.m_external_Q.begin(),  modelstates.m_external_Q.end(),  0.f);
    fill(modelstates.m_external_Q0.begin(), modelstates.m_external_Q0.end(), 0.f);
    // BC:HFlux
    for (size_t i = 0; i < model.m_hflux_idx.size(); i++) { modelstates.m_external_Q0[model.m_hflux_idx[i]] += model.m_hflux_mag[i]; }
    // BC:BodyHFlux
    for (size_t i = 0; i < model.m_bhflux_idx.size(); i++) { modelstates.m_external_Q0[model.m_bhflux_idx[i]] += model.m_bhflux_mag[i]; }
    // BC:Metabo
    for (size_t i = 0; i < model.m_metabo_mag.size(); i++) { modelstates.m_external_Q0[i] += model.m_metabo_mag[i]; }
    // BC:FixT
    for (size_t i = 0; i < model.m_fixT_idx.size(); i++) { modelstates.m_fixT_flag[model.m_fixT_idx[i]] = true; modelstates.m_fixT_mag[model.m_fixT_idx[i]] = model.m_fixT_mag[i]; }
    modelstates.m_external_Q = modelstates.m_external_Q0;
}

void computeRunTimeBC(const Model& model, ModelStates& modelstates, const size_t curr_step)
{
    // BC:Disp
    const float n((curr_step + 1) * model.m_dt / model.m_total_t);
    for (size_t i = 0; i < model.m_disp_idx_x.size(); i++) { modelstates.m_disp_mag_t[model.m_disp_idx_x[i] * 3 + 0] = model.m_disp_mag_x[i] * n; }
    for (size_t i = 0; i < model.m_disp_idx_y.size(); i++) { modelstates.m_disp_mag_t[model.m_disp_idx_y[i] * 3 + 1] = model.m_disp_mag_y[i] * n; }
    for (size_t i = 0; i < model.m_disp_idx_z.size(); i++) { modelstates.m_disp_mag_t[model.m_disp_idx_z[i] * 3 + 2] = model.m_disp_mag_z[i] * n; }
    // BC:Perfu
    for (size_t i = 0; i < model.m_perfu_idx.size(); i++) { modelstates.m_external_Q[model.m_perfu_idx[i]] = modelstates.m_external_Q0[model.m_perfu_idx[i]] - model.m_perfu_const1[i] * (modelstates.m_curr_T[model.m_perfu_idx[i]] - model.m_perfu_refT[i]); }
}

bool computeOneStep(const Model& model, ModelStates& modelstates)
{
    bool no_err(true);
#pragma omp parallel num_threads(NUM_THREADS)
    {
        int id = omp_get_thread_num();
        float u[3][4], C[3][3], invC[3][3], Jsq(0.f), J(0.f), XSVol[3][3], f[3][4],
              temp_X[3][3], invX[3][3],
              T_diff(0.f),
              invX_expan[3][3], J_invX_expan(0.f),
              temp33[3][3], temp34[3][4];
        for (int i = id; i < model.m_tets.size(); i += NUM_THREADS) // loop through tets to compute for force and thermal load contributions
        {
            T4 *tet = model.m_tets[i];
            for (size_t m = 0; m < 4; m++) { for (size_t n = 0; n < 3; n++) { u[n][m] = modelstates.m_curr_U[tet->m_n_idx[m] * 3 + n]; } }
            mat34x34T(u, tet->m_DHDX, tet->m_X); tet->m_X[0][0] += 1.f; tet->m_X[1][1] += 1.f; tet->m_X[2][2] += 1.f;
            memcpy(temp_X, tet->m_X, sizeof(float) * 3 * 3);
            // compute X_expan and elastic defor.grad
            if (tet->m_T_expan_vals.size() != 0)
            {
                T_diff = (modelstates.m_curr_T[tet->m_n_idx[0]] + modelstates.m_curr_T[tet->m_n_idx[1]] + modelstates.m_curr_T[tet->m_n_idx[2]] + modelstates.m_curr_T[tet->m_n_idx[3]]) / 4.f - model.m_T0;
                if (tet->m_T_expan_type == "T_EXPAN_ISO")
                {
                    float lamda_i(1.f + tet->m_T_expan_vals[0] * T_diff);
                    tet->m_X_expan[0][0] = lamda_i; tet->m_X_expan[1][1] = lamda_i; tet->m_X_expan[2][2] = lamda_i;
                }
                else if (tet->m_T_expan_type == "T_EXPAN_TI")
                {
                    float lamda_i(1.f + tet->m_T_expan_vals[0] * T_diff);
                    float lamda_m_minus_i(tet->m_T_expan_vals[1] * T_diff);
                    tet->m_X_expan[0][0] = lamda_m_minus_i * tet->m_T_expan_vals[2] + lamda_i; tet->m_X_expan[0][1] = lamda_m_minus_i * tet->m_T_expan_vals[3];           tet->m_X_expan[0][2] = lamda_m_minus_i * tet->m_T_expan_vals[4];
                    tet->m_X_expan[1][0] = tet->m_X_expan[0][1];                               tet->m_X_expan[1][1] = lamda_m_minus_i * tet->m_T_expan_vals[5] + lamda_i; tet->m_X_expan[1][2] = lamda_m_minus_i * tet->m_T_expan_vals[6];
                    tet->m_X_expan[2][0] = tet->m_X_expan[0][2];                               tet->m_X_expan[2][1] = tet->m_X_expan[1][2];                               tet->m_X_expan[2][2] = lamda_m_minus_i * tet->m_T_expan_vals[7] + lamda_i;
                }
                else if (tet->m_T_expan_type == "T_EXPAN_ORTHO")
                {
                    float lamda_i(1.f + tet->m_T_expan_vals[0] * T_diff);
                    float lamda_m_minus_i(tet->m_T_expan_vals[1] * T_diff);
                    float lamda_n_minus_i(tet->m_T_expan_vals[8] * T_diff);
                    tet->m_X_expan[0][0] = lamda_m_minus_i * tet->m_T_expan_vals[2] + lamda_n_minus_i * tet->m_T_expan_vals[9] + lamda_i; tet->m_X_expan[0][1] = lamda_m_minus_i * tet->m_T_expan_vals[3] + lamda_n_minus_i * tet->m_T_expan_vals[10];           tet->m_X_expan[0][2] = lamda_m_minus_i * tet->m_T_expan_vals[4] + lamda_n_minus_i * tet->m_T_expan_vals[11];
                    tet->m_X_expan[1][0] = tet->m_X_expan[0][1];                                                                          tet->m_X_expan[1][1] = lamda_m_minus_i * tet->m_T_expan_vals[5] + lamda_n_minus_i * tet->m_T_expan_vals[12] + lamda_i; tet->m_X_expan[1][2] = lamda_m_minus_i * tet->m_T_expan_vals[6] + lamda_n_minus_i * tet->m_T_expan_vals[13];
                    tet->m_X_expan[2][0] = tet->m_X_expan[0][2];                                                                          tet->m_X_expan[2][1] = tet->m_X_expan[1][2];                                                                           tet->m_X_expan[2][2] = lamda_m_minus_i * tet->m_T_expan_vals[7] + lamda_n_minus_i * tet->m_T_expan_vals[14] + lamda_i;
                }
                else if (tet->m_T_expan_type == "other_expansion_types") { /*add your code here*/ }
                matInv33(tet->m_X_expan, invX_expan, J_invX_expan);
                mat33x33(tet->m_X, invX_expan, temp_X); // elastic defor.grad
            }
            mat33Tx33(temp_X, temp_X, C); // C: right Cauchy-Green tensor
            matInv33(C, invC, Jsq); J = sqrt(Jsq);
            if (tet->m_M_material_type == "NH")
            {
                const float J23(powf(J, -0.66666667f)), // J^(-2/3)
                            I1(C[0][0] + C[1][1] + C[2][2]),
                            const1(J23 * tet->m_M_material_vals[0]), // J23*Mu
                            const2(-const1 * I1 / 3.f + tet->m_M_material_vals[1] * J * (J - 1.f)); // -Mu*J23*I1/3 + K*J*(J-1)
                tet->m_S[0][0] = const2 * invC[0][0] + const1; tet->m_S[0][1] = const2 * invC[0][1];          tet->m_S[0][2] = const2 * invC[0][2];
                tet->m_S[1][0] = tet->m_S[0][1];               tet->m_S[1][1] = const2 * invC[1][1] + const1; tet->m_S[1][2] = const2 * invC[1][2];
                tet->m_S[2][0] = tet->m_S[0][2];               tet->m_S[2][1] = tet->m_S[1][2];               tet->m_S[2][2] = const2 * invC[2][2] + const1;
            }
            else if (tet->m_M_material_type == "TI")
            {
                const float J23(powf(J, -0.66666667f)),
                            I1(C[0][0] + C[1][1] + C[2][2]),
                            I4(tet->m_M_material_vals[3] * C[0][0] + 2.f * tet->m_M_material_vals[4] * C[0][1] + 2.f * tet->m_M_material_vals[5] * C[0][2] + tet->m_M_material_vals[6] * C[1][1] + 2.f * tet->m_M_material_vals[7] * C[1][2] + tet->m_M_material_vals[8] * C[2][2]),
                            I4cap(J23 * I4),
                            const1(J23 * tet->m_M_material_vals[0]),
                            const2(tet->m_M_material_vals[2] * (I4cap - 1.f)),
                            const3(2.f * J23 * const2),
                            const4(-(const1 * I1 + 2.f * const2 * I4cap) / 3.f + tet->m_M_material_vals[1] * J * (J - 1.f)); // -(Mu*J23*I1+2*Eta*(I4cap-1)*I4cap)/3 + K*J*(J-1)
                tet->m_S[0][0] = const4 * invC[0][0] + const3 * tet->m_M_material_vals[3] + const1; tet->m_S[0][1] = const4 * invC[0][1] + const3 * tet->m_M_material_vals[4];          tet->m_S[0][2] = const4 * invC[0][2] + const3 * tet->m_M_material_vals[5];
                tet->m_S[1][0] = tet->m_S[0][1];                                                    tet->m_S[1][1] = const4 * invC[1][1] + const3 * tet->m_M_material_vals[6] + const1; tet->m_S[1][2] = const4 * invC[1][2] + const3 * tet->m_M_material_vals[7];
                tet->m_S[2][0] = tet->m_S[0][2];                                                    tet->m_S[2][1] = tet->m_S[1][2];                                                    tet->m_S[2][2] = const4 * invC[2][2] + const3 * tet->m_M_material_vals[8] + const1;
            }
            else if (tet->m_M_material_type == "other_material_types") { /*add your code here*/ }
            if (tet->m_T_expan_vals.size() != 0)
            {
                mat33x33(invX_expan, tet->m_S, temp33);
                mat33x33T(temp33, invX_expan, tet->m_S);
                mat33xScalar(tet->m_S, J_invX_expan, tet->m_S);
            }
            // compute ele f
            mat33x33(tet->m_X, tet->m_S, XSVol);
            mat33xScalar(XSVol, tet->m_Vol, XSVol);
            mat33x34(XSVol, tet->m_DHDX, f);
            for (size_t m = 0; m < 4; m++) { for (size_t n = 0; n < 3; n++) { modelstates.m_ele_nodal_internal_F[tet->m_idx * 12 + m * 3 + n] = f[n][m]; } }

            // compute DHDx and vol for the deformed state
            matInv33(tet->m_X, invX, J);
            mat33Tx34(invX, tet->m_DHDX, tet->m_DHDx);
            tet->m_vol = tet->m_Vol * J;
            if (tet->m_T_material_type == "T_ISO")
            {
                mat34Tx34(tet->m_DHDx, tet->m_DHDx, tet->m_K);
                mat44xScalar(tet->m_K, tet->m_vol * tet->m_D[0][0], tet->m_K);
            }
            else
            {
                mat33x34(tet->m_D, tet->m_DHDx, temp34);
                mat34Tx34(tet->m_DHDx, temp34, tet->m_K);
                mat44xScalar(tet->m_K, tet->m_vol, tet->m_K);
            }
            // compute ele q
            for (size_t m = 0; m < 4; m++) { modelstates.m_ele_nodal_internal_Q[tet->m_idx * 4 + m] = tet->m_K[m][0] * modelstates.m_curr_T[tet->m_n_idx[0]]
                                                                                                    + tet->m_K[m][1] * modelstates.m_curr_T[tet->m_n_idx[1]]
                                                                                                    + tet->m_K[m][2] * modelstates.m_curr_T[tet->m_n_idx[2]]
                                                                                                    + tet->m_K[m][3] * modelstates.m_curr_T[tet->m_n_idx[3]]; }
        }
#pragma omp barrier
        for (int i = id; i < model.m_nodes.size(); i += NUM_THREADS) // loop through nodes to compute for new displacements U and temperatures T
        {
            // assemble nodal forces and thermal loads from individual ele nodal forces and thermal loads, due to avoiding race condition
            float nodal_internal_F[3] = { 0.f, 0.f, 0.f }, nodal_internal_Q(0.f);
            unsigned int tracking_num_eles(model.m_tracking_num_eles_i_eles_per_node_j[i * 2 + 0]),
                         eles_per_node    (model.m_tracking_num_eles_i_eles_per_node_j[i * 2 + 1]),
                         ele_idx(0), node_local_idx(0);
            for (unsigned int j = 0; j < eles_per_node; j++)
            {
                ele_idx        = model.m_ele_node_local_idx_pair[(tracking_num_eles + j) * 2 + 0];
                node_local_idx = model.m_ele_node_local_idx_pair[(tracking_num_eles + j) * 2 + 1];
                nodal_internal_F[0] += modelstates.m_ele_nodal_internal_F[ele_idx * 12 + node_local_idx * 3 + 0];
                nodal_internal_F[1] += modelstates.m_ele_nodal_internal_F[ele_idx * 12 + node_local_idx * 3 + 1];
                nodal_internal_F[2] += modelstates.m_ele_nodal_internal_F[ele_idx * 12 + node_local_idx * 3 + 2];
                nodal_internal_Q += modelstates.m_ele_nodal_internal_Q[ele_idx * 4 + node_local_idx];
            }
            size_t n_DOF(0);
            for (size_t j = 0; j < 3; j++)
            {
                n_DOF = i * 3 + j;
                if (modelstates.m_disp_mag_t[n_DOF] != 0.f) { modelstates.m_next_U[n_DOF] = modelstates.m_disp_mag_t[n_DOF]; } // apply BC:Disp
                else if (modelstates.m_fixP_flag[n_DOF] == true) { modelstates.m_next_U[n_DOF] = 0.f; }                        // apply BC:FixP
                else                                                                                                           // explicit central-difference integration
                {
                    modelstates.m_next_U[n_DOF] = modelstates.m_central_diff_const1[n_DOF] * (modelstates.m_external_F[n_DOF] - nodal_internal_F[j]) +
                                                  modelstates.m_central_diff_const2[n_DOF] * modelstates.m_curr_U[n_DOF] +
                                                  modelstates.m_central_diff_const3[n_DOF] * modelstates.m_prev_U[n_DOF];
                    if (isnan(modelstates.m_next_U[n_DOF])) { no_err = false; }
                }
            }
            if (modelstates.m_fixT_flag[i] == true) { modelstates.m_next_T[i] = modelstates.m_fixT_mag[i]; } // apply BC:FixT
            else                                                                                             // explicit time integration
            {
                modelstates.m_next_T[i] = modelstates.m_curr_T[i] + modelstates.m_constA[i] * (modelstates.m_external_Q[i] - nodal_internal_Q);
                if (isnan(modelstates.m_next_T[i])) { no_err = false; }
            }
        }
    }
    if (!no_err) { cerr << "\n\tError: solution diverged, simulation aborted. Try a smaller time step." << endl; }
    else
    {
        modelstates.m_prev_U.swap(modelstates.m_curr_U); modelstates.m_curr_U.swap(modelstates.m_next_U);
        modelstates.m_curr_T.swap(modelstates.m_next_T);
    }
    return no_err;
}

int exportVTK(const Model& model, const ModelStates& modelstates)
{
    const vector<string> outputs{ "U.vtk", "Undeformed.vtk", "T.vtk" }; // other outputs can be added by the user, e.g., S.vtk where 2nd PK stresses are stored in tet->m_S
    cout << "\n\texporting..." << endl;
    for (string vtk : outputs)
    {
        ofstream fout(vtk.c_str());
        if (fout.is_open())
        {
            fout << "# vtk DataFile Version 3.8" << endl;
            fout << vtk.c_str() << endl;
            fout << "ASCII" << endl;
            fout << "DATASET UNSTRUCTURED_GRID" << endl;
            fout << "POINTS " << model.m_nodes.size() << " float" << endl;
            if (vtk == "Undeformed.vtk") { for (Node* node : model.m_nodes) { fout << node->m_x << " " << node->m_y << " " << node->m_z << endl; } }
            else { for (Node* node : model.m_nodes) { fout << node->m_x + modelstates.m_curr_U[node->m_idx * 3 + 0] << " " << node->m_y + modelstates.m_curr_U[node->m_idx * 3 + 1] << " " << node->m_z + modelstates.m_curr_U[node->m_idx * 3 + 2] << endl; } }
            fout << "CELLS " << model.m_tets.size() << " " << model.m_tets.size() * (4 + 1) << endl;
            for (T4* tet : model.m_tets) { fout << 4 << " " << tet->m_n_idx[0] << " " << tet->m_n_idx[1] << " " << tet->m_n_idx[2] << " " << tet->m_n_idx[3] << endl; }
            fout << "CELL_TYPES " << model.m_tets.size() << endl;
            for (size_t i = 0; i < model.m_tets.size(); i++) { fout << 10 << endl; }
            fout << "POINT_DATA " << model.m_nodes.size() << endl;
            if (vtk == "U.vtk" || vtk == "Undeformed.vtk")
            {
                fout << "VECTORS " << vtk.c_str() << " float" << endl;
                for (Node* node : model.m_nodes) { fout << modelstates.m_curr_U[node->m_idx * 3 + 0] << " " << modelstates.m_curr_U[node->m_idx * 3 + 1] << " " << modelstates.m_curr_U[node->m_idx * 3 + 2] << endl; }
            }
            else if (vtk == "T.vtk")
            {
                fout << "SCALARS " << vtk.c_str() << " float" << endl;
                fout << "LOOKUP_TABLE default" << endl;
                for (Node* node : model.m_nodes) { fout << modelstates.m_curr_T[node->m_idx] << endl; }
            }
            cout << "\t\t\t" << vtk.c_str() << endl;
        }
        else { cerr << "\n\tError: cannot open " << vtk.c_str() << " for writing, results not saved." << endl; return EXIT_FAILURE; }
    }
    cout << "\tVTK saved." << endl;
    return EXIT_SUCCESS;
}

void mat33x33(const float A[3][3], const float B[3][3], float AB[3][3])
{
    memset(AB, 0, sizeof(float) * 3 * 3);
    for (size_t i = 0; i < 3; i++) { for (size_t j = 0; j < 3; j++) { for (size_t k = 0; k < 3; k++) { AB[i][j] += A[i][k] * B[k][j]; } } }
}
void mat33x34(const float A[3][3], const float B[3][4], float AB[3][4])
{
    memset(AB, 0, sizeof(float) * 3 * 4);
    for (size_t i = 0; i < 3; i++) { for (size_t j = 0; j < 4; j++) { for (size_t k = 0; k < 3; k++) { AB[i][j] += A[i][k] * B[k][j]; } } }
}
void mat33Tx33(const float A[3][3], const float B[3][3], float AB[3][3])
{
    memset(AB, 0, sizeof(float) * 3 * 3);
    for (size_t i = 0; i < 3; i++) { for (size_t j = 0; j < 3; j++) { for (size_t k = 0; k < 3; k++) { AB[i][j] += A[k][i] * B[k][j]; } } }
}
void mat33Tx34(const float A[3][3], const float B[3][4], float AB[3][4])
{
    memset(AB, 0, sizeof(float) * 3 * 4);
    for (size_t i = 0; i < 3; i++) { for (size_t j = 0; j < 4; j++) { for (size_t k = 0; k < 3; k++) { AB[i][j] += A[k][i] * B[k][j]; } } }
}
void mat34Tx34(const float A[3][4], const float B[3][4], float AB[4][4])
{
    memset(AB, 0, sizeof(float) * 4 * 4);
    for (size_t i = 0; i < 4; i++) { for (size_t j = 0; j < 4; j++) { for (size_t k = 0; k < 3; k++) { AB[i][j] += A[k][i] * B[k][j]; } } }
}
void mat33x33T(const float A[3][3], const float B[3][3], float AB[3][3])
{
    memset(AB, 0, sizeof(float) * 3 * 3);
    for (size_t i = 0; i < 3; i++) { for (size_t j = 0; j < 3; j++) { for (size_t k = 0; k < 3; k++) { AB[i][j] += A[i][k] * B[j][k]; } } }
}
void mat34x34T(const float A[3][4], const float B[3][4], float AB[3][3])
{
    memset(AB, 0, sizeof(float) * 3 * 3);
    for (size_t i = 0; i < 3; i++) { for (size_t j = 0; j < 3; j++) { for (size_t k = 0; k < 4; k++) { AB[i][j] += A[i][k] * B[j][k]; } } }
}
void mat33xScalar(const float A[3][3], const float b, float Ab[3][3])
{
    for (size_t i = 0; i < 3; i++) { for (size_t j = 0; j < 3; j++) { Ab[i][j] = A[i][j] * b; } }
}
void mat44xScalar(const float A[4][4], const float b, float Ab[4][4])
{
    for (size_t i = 0; i < 4; i++) { for (size_t j = 0; j < 4; j++) { Ab[i][j] = A[i][j] * b; } }
}
void matDet33(const float A[3][3], float &detA)
{
    detA = A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) - A[1][0] * (A[0][1] * A[2][2] - A[0][2] * A[2][1]) + A[2][0] * (A[0][1] * A[1][2] - A[0][2] * A[1][1]);
}
void matInv33(const float A[3][3], float invA[3][3], float &detA)
{
    matDet33(A, detA);
    invA[0][0] = (A[1][1] * A[2][2] - A[1][2] * A[2][1]) / detA; invA[0][1] = (A[0][2] * A[2][1] - A[0][1] * A[2][2]) / detA; invA[0][2] = (A[0][1] * A[1][2] - A[0][2] * A[1][1]) / detA;
    invA[1][0] = (A[1][2] * A[2][0] - A[1][0] * A[2][2]) / detA; invA[1][1] = (A[0][0] * A[2][2] - A[0][2] * A[2][0]) / detA; invA[1][2] = (A[0][2] * A[1][0] - A[0][0] * A[1][2]) / detA;
    invA[2][0] = (A[1][0] * A[2][1] - A[1][1] * A[2][0]) / detA; invA[2][1] = (A[0][1] * A[2][0] - A[0][0] * A[2][1]) / detA; invA[2][2] = (A[0][0] * A[1][1] - A[0][1] * A[1][0]) / detA;
}
