#include "efield.h"
#include "../module_base/constants.h"
#include "../module_base/timer.h"
#include "../module_base/global_variable.h"
#include "../src_parallel/parallel_reduce.h"

double Efield::etotefield = 0.0;
double Efield::tot_dipole = 0.0;
int Efield::efield_dir;
double Efield::efield_pos_max;
double Efield::efield_pos_dec;
double Efield::efield_amp ;
double Efield::bvec[3];
double Efield::bmod;

Efield::Efield(){}

Efield::~Efield(){}

//=======================================================
// calculate dipole potential in surface calculations
//=======================================================
ModuleBase::matrix Efield::add_efield(const UnitCell &cell, 
                                        PW_Basis &pwb, 
                                        const int &nspin, 
                                        const double *const *const rho)
{
    ModuleBase::TITLE("Efield", "add_efield");
    ModuleBase::timer::tick("Efield", "add_efield");

    double latvec;    // latvec along the efield direction
    if(efield_dir == 0)
    {
        bvec[0] = cell.G.e11;
        bvec[1] = cell.G.e12; 
        bvec[2] = cell.G.e13; 
        latvec = cell.a1.norm();
    }
    else if(efield_dir == 1)
    {
        bvec[0] = cell.G.e21;
        bvec[1] = cell.G.e22; 
        bvec[2] = cell.G.e23; 
        latvec = cell.a2.norm();
    }
    else if(efield_dir = 2)
    {
        bvec[0] = cell.G.e31;
        bvec[1] = cell.G.e32; 
        bvec[2] = cell.G.e33; 
        latvec = cell.a3.norm();
    }
    else
    {
        ModuleBase::WARNING_QUIT("Efield::add_efield", "direction is wrong!");
    }
    bmod = sqrt(pow(bvec[0],2) + pow(bvec[1],2) + pow(bvec[2],2));

    double ion_dipole = 0;
    double elec_dipole = 0;

    if(GlobalV::DIP_COR_FLAG)
    {
        ion_dipole = cal_ion_dipole(cell, bmod);
        elec_dipole = cal_elec_dipole(cell, pwb, nspin, rho, bmod);
        tot_dipole = ion_dipole - elec_dipole;

        // energy correction
        etotefield = - ModuleBase::e2 * (efield_amp  - 0.5 * tot_dipole) * tot_dipole * cell.omega / ModuleBase::FOUR_PI;
    }
    else
    {
        ion_dipole = cal_ion_dipole(cell, bmod);

        // energy correction
        etotefield = - ModuleBase::e2 * efield_amp  * ion_dipole * cell.omega / ModuleBase::FOUR_PI;
    }

    const double length = (1.0 - efield_pos_dec) * latvec * cell.lat0;
    const double vamp = ModuleBase::e2 * (efield_amp  - tot_dipole) * length;

    GlobalV::ofs_running << "\n\n Adding external electric field: " << std::endl;
    if(GlobalV::DIP_COR_FLAG)
    {
        ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running, "Computed dipole along efield_dir", efield_dir);
        ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running, "Elec. dipole (Ry a.u.)", elec_dipole);
        ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running, "Ion dipole (Ry a.u.)", ion_dipole);
        ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running, "Total dipole (Ry a.u.)", tot_dipole);
    }
    if( abs(efield_amp ) > 0.0) 
    {
        ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running, "Amplitute of Efield (Hartree)", efield_amp );
    }
    ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running, "Potential amplitute (Ry)", vamp);
    ModuleBase::GlobalFunc::OUT(GlobalV::ofs_running, "Total length (Bohr)", length);

    // dipole potential
    ModuleBase::matrix v(nspin, pwb.nrxx);
    const int nspin0 = (nspin == 2) ? 2 : 1;

    for (int ir = 0; ir < pwb.nrxx; ++ir)
    {
        int i = ir / (pwb.ncy * pwb.nczp);
        int j = ir / pwb.nczp - i * pwb.ncy;
        int k = ir % pwb.nczp + pwb.nczp_start;
        double x = (double)i / pwb.ncx;
        double y = (double)j / pwb.ncy;
        double z = (double)k / pwb.ncz;
        ModuleBase::Vector3<double> pos(x, y, z);

        double saw = saw_function(efield_pos_max, efield_pos_dec, pos[efield_dir]);

        for (int is = 0; is < nspin0; is++)
        {
            v(is, ir) = saw;
        }
    }

    double fac = ModuleBase::e2 * (efield_amp  - tot_dipole) * cell.lat0 / bmod;

    ModuleBase::timer::tick("Efield", "add_efield");
    return v * fac;
}


//=======================================================
// calculate dipole density in surface calculations
//=======================================================
double Efield::cal_ion_dipole(const UnitCell &cell, const double &bmod)
{
    double ion_dipole = 0;
    for(int it=0; it<cell.ntype; ++it)
    {
        double sum = 0;
        for(int ia=0; ia<cell.atoms[it].na; ++ia)
        {
            sum += saw_function(efield_pos_max, efield_pos_dec, cell.atoms[it].taud[ia][efield_dir]);
        }
        ion_dipole += sum * cell.atoms[it].zv;
    }
    ion_dipole *= cell.lat0 / bmod * ModuleBase::FOUR_PI / cell.omega;

    return ion_dipole;
}

double Efield::cal_elec_dipole(const UnitCell &cell, 
                            PW_Basis &pwb, 
                            const int &nspin, 
                            const double *const *const rho,
                            const double &bmod)
{
    double elec_dipole = 0;
    const int nspin0 = (nspin == 2) ? 2 : 1;

    for (int ir = 0; ir < pwb.nrxx; ++ir)
    {
        int i = ir / (pwb.ncy * pwb.nczp);
        int j = ir / pwb.nczp - i * pwb.ncy;
        int k = ir % pwb.nczp + pwb.nczp_start;
        double x = (double)i / pwb.ncx;
        double y = (double)j / pwb.ncy;
        double z = (double)k / pwb.ncz;
        ModuleBase::Vector3<double> pos(x, y, z);

        double saw = saw_function(efield_pos_max, efield_pos_dec, pos[efield_dir]);

        for (int is = 0; is < nspin0; is++)
        {
            elec_dipole += rho[is][ir] * saw;
        }
    }

    Parallel_Reduce::reduce_double_pool(elec_dipole);
    elec_dipole *= cell.lat0 / bmod * ModuleBase::FOUR_PI / pwb.ncxyz;

    return elec_dipole;
}

double Efield::saw_function(const double &a, const double &b, const double &x)
{
    assert(x>=0);
    assert(x<=1);

    const double fac = 1 - b;

    if( x < a )
    {
        return x - a + 0.5 * fac;
    }
    else if( x > (a+b))
    {
        return x - a - 1 + 0.5 * fac;
    }
    else
    {
        return 0.5 * fac - fac * (x - a) / b;
    }
}

void Efield::compute_force(const UnitCell &cell, ModuleBase::matrix &fdip)
{
    if(GlobalV::DIP_COR_FLAG)
    {
        int iat = 0;
        for(int it=0; it<cell.ntype; ++it)
        {
            for(int ia=0; ia<cell.atoms[it].na; ++ia)
            {
                for(int jj=0; jj<3; ++jj)
                {
                    fdip(iat, jj) = ModuleBase::e2 * (efield_amp  - tot_dipole) * cell.atoms[it].zv * bvec[jj] / bmod;
                }
                ++iat;
            }
        }
    }
    else
    {
        int iat = 0;
        for(int it=0; it<cell.ntype; ++it)
        {
            for(int ia=0; ia<cell.atoms[it].na; ++ia)
            {
                for(int jj=0; jj<3; ++jj)
                {
                    fdip(iat, jj) = ModuleBase::e2 * efield_amp  * cell.atoms[it].zv * bvec[jj] / bmod;
                }
                ++iat;
            }
        }
    }
}