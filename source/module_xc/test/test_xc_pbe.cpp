#include "../xc_functional.h"
#include "gtest/gtest.h"

//a mock function of WARNING_QUIT, to avoid the uncorrected call by matrix.cpp at line 37.
namespace ModuleBase
{
    void WARNING_QUIT(const std::string &file,const std::string &description) {return ;}
}

namespace GlobalV
{
    std::string BASIS_TYPE = "";
    bool STRESS = 0;
    int NSPIN = 1;
}

class XCTest_PBE : public testing::Test
{
    protected:
        std::vector<double> e_lda, v_lda;
        std::vector<double> e_gga, v1_gga, v2_gga;

        void SetUp()
        {
            XC_Functional::set_xc_type("PBE");
            std::vector<double> rho  = {0.17E+01, 0.17E+01, 0.15E+01, 0.88E-01, 0.18E+04};
            std::vector<double> grho = {0.81E-11, 0.17E+01, 0.36E+02, 0.87E-01, 0.55E+00};

            for(int i=0;i<5;i++)
            {
                double e,v,v1,v2;
                XC_Functional::xc(rho[i],e,v);
                e_lda.push_back(e);
                v_lda.push_back(v);
                XC_Functional::gcxc(rho[i],grho[i],e,v1,v2);
                e_gga.push_back(e);
                v1_gga.push_back(v1);
                v2_gga.push_back(v2);
            }
        }
};

TEST_F(XCTest_PBE, set_xc_type)
{
    EXPECT_EQ(XC_Functional::get_func_type(),2);
    std::vector<double> e_lda_ref  = {-0.9570906378,-0.9570906378,-0.9200171474,-0.3808291731,-9.124862983};
    std::vector<double> v_lda_ref  = {-1.259358955 , -1.259358955, -1.210234884,-0.4975768336,-12.12952188};
    std::vector<double> e_gga_ref  = {0.0          , -0.000103750,-0.0328708695,-0.0032277985,0.0         };
    std::vector<double> v1_gga_ref = {0.0          ,0.00021536874,0.04931694948,0.05374316118,0.0         };
    std::vector<double> v2_gga_ref = {0.0          ,-0.0002386176,-0.0025842562,-0.0825164089,0.0         };

    for (int i = 0;i<5;++i)
    {
        EXPECT_NEAR(e_lda[i],e_lda_ref[i],1.0e-8);
        EXPECT_NEAR(v_lda[i],v_lda_ref[i],1.0e-8);
        EXPECT_NEAR(e_gga[i],e_gga_ref[i],1.0e-8);
        EXPECT_NEAR(v1_gga[i],v1_gga_ref[i],1.0e-8);
        EXPECT_NEAR(v2_gga[i],v2_gga_ref[i],1.0e-8);
    }
}