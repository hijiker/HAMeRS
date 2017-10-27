#include "apps/Navier-Stokes/NavierStokesInitialConditions.hpp"

#include <sstream>

/*
 * Set the data on the patch interior to some initial values.
 */
void
NavierStokesInitialConditions::initializeDataOnPatch(
    hier::Patch& patch,
    const std::vector<boost::shared_ptr<pdat::CellVariable<double> > >& conservative_variables,
    const boost::shared_ptr<hier::VariableContext>& data_context,
    const double data_time,
    const bool initial_time)
{
    NULL_USE(data_time);
    
    if (d_dim != tbox::Dimension(2))
    {
        TBOX_ERROR(d_object_name
            << ": "
            << "Dimension of problem should be 2!"
            << std::endl);
    }
    
    if (d_flow_model_type != FLOW_MODEL::FOUR_EQN_CONSERVATIVE)
    {
        TBOX_ERROR(d_object_name
            << ": "
            << "Flow model should be conservative four-equation model!"
            << std::endl);
    }
    
    if (d_num_species != 2)
    {
        TBOX_ERROR(d_object_name
            << ": "
            << "Number of species should be 2!"
            << std::endl);
    }
    
    if (initial_time)
    {
        const boost::shared_ptr<geom::CartesianPatchGeometry> patch_geom(
            BOOST_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
                patch.getPatchGeometry()));
        
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
        TBOX_ASSERT(patch_geom);
#endif
        
        const double* const dx = patch_geom->getDx();
        const double* const patch_xlo = patch_geom->getXLower();
        
        // Get the dimensions of box that covers the interior of Patch.
        hier::Box patch_box = patch.getBox();
        const hier::IntVector patch_dims = patch_box.numberCells();
        
        /*
         * Initialize data for 2D binary mass diffusion problem.
         */
        
        boost::shared_ptr<pdat::CellVariable<double> > var_partial_density = conservative_variables[0];
        boost::shared_ptr<pdat::CellVariable<double> > var_momentum        = conservative_variables[1];
        boost::shared_ptr<pdat::CellVariable<double> > var_total_energy    = conservative_variables[2];
        
        boost::shared_ptr<pdat::CellData<double> > partial_density(
            BOOST_CAST<pdat::CellData<double>, hier::PatchData>(
                patch.getPatchData(var_partial_density, data_context)));
        
        boost::shared_ptr<pdat::CellData<double> > momentum(
            BOOST_CAST<pdat::CellData<double>, hier::PatchData>(
                patch.getPatchData(var_momentum, data_context)));
        
        boost::shared_ptr<pdat::CellData<double> > total_energy(
            BOOST_CAST<pdat::CellData<double>, hier::PatchData>(
                patch.getPatchData(var_total_energy, data_context)));
        
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
        TBOX_ASSERT(partial_density);
        TBOX_ASSERT(momentum);
        TBOX_ASSERT(total_energy);
#endif
        
        double* rho_Y_1 = partial_density->getPointer(0);
        double* rho_Y_2 = partial_density->getPointer(1);
        double* rho_u   = momentum->getPointer(0);
        double* rho_v   = momentum->getPointer(1);
        double* E       = total_energy->getPointer(0);
        
        if (d_project_name.find("2D Poggi's RMI 1mm smooth interface reduced domain size") != std::string::npos)
        {
            /*
             * Get the settings.
             */
            
            std::string settings = d_project_name.substr(56);
            
            std::stringstream ss(settings);
            
            std::string A_idx_str;
            std::string m_idx_str;
            std::string random_seed_str;
            
            std::getline(ss, A_idx_str, '-');
            std::getline(ss, m_idx_str, '-');
            std::getline(ss, random_seed_str);
            
            double A_candidates[] = {0.01e-3, sqrt(2.0)*0.01e-3, 0.02e-3};
            int m_min_candidates[] = {25, 20, 15, 10};
            int m_max_candidates[] = {30, 30, 30, 30};
            
            // Seed for "random" phase shifts.
            int random_seed = std::stoi(random_seed_str) - 1;
            
            double phase_shifts[21];
            if (random_seed == 0)
            {
                phase_shifts[0] = 5.119059895756811000e+000;
                phase_shifts[1] = 5.691258590395267300e+000;
                phase_shifts[2] = 7.978816983408705300e-001;
                phase_shifts[3] = 5.738909759225261800e+000;
                phase_shifts[4] = 3.973230324742651500e+000;
                phase_shifts[5] = 6.128644395486362300e-001;
                phase_shifts[6] = 1.749855916861123200e+000;
                phase_shifts[7] = 3.436157926236805200e+000;
                phase_shifts[8] = 6.016192879924800800e+000;
                phase_shifts[9] = 6.062573467430127000e+000;
                phase_shifts[10] = 9.903121990156674700e-001;
                phase_shifts[11] = 6.098414305612863000e+000;
                phase_shifts[12] = 6.014057305717998700e+000;
                phase_shifts[13] = 3.049705144518116000e+000;
                phase_shifts[14] = 5.028310483744898600e+000;
                phase_shifts[15] = 8.914981581520268200e-001;
                phase_shifts[16] = 2.650004294134627800e+000;
                phase_shifts[17] = 5.753735997130328400e+000;
                phase_shifts[18] = 4.977585453328568800e+000;
                phase_shifts[19] = 6.028668715861979200e+000;
                phase_shifts[20] = 4.120140326260335300e+000;
            }
            else
            {
                phase_shifts[0] = 2.620226532717789200e+000;
                phase_shifts[1] = 4.525932273597345700e+000;
                phase_shifts[2] = 7.186381718527406600e-004;
                phase_shifts[3] = 1.899611578242180700e+000;
                phase_shifts[4] = 9.220944569241362700e-001;
                phase_shifts[5] = 5.801805019369201700e-001;
                phase_shifts[6] = 1.170307423440345900e+000;
                phase_shifts[7] = 2.171222082895173200e+000;
                phase_shifts[8] = 2.492963564452900500e+000;
                phase_shifts[9] = 3.385485386352383500e+000;
                phase_shifts[10] = 2.633876813749063600e+000;
                phase_shifts[11] = 4.305361097085856200e+000;
                phase_shifts[12] = 1.284611371532881700e+000;
                phase_shifts[13] = 5.517374574309792800e+000;
                phase_shifts[14] = 1.720813231802212400e-001;
                phase_shifts[15] = 4.212671608894216200e+000;
                phase_shifts[16] = 2.622003402848613000e+000;
                phase_shifts[17] = 3.510351721361030500e+000;
                phase_shifts[18] = 8.820771499014955500e-001;
                phase_shifts[19] = 1.244708365548507600e+000;
                phase_shifts[20] = 5.031226508705986900e+000;
            }
            
            // Amplitude.
            double A = A_candidates[std::stoi(A_idx_str)];
            
            // Bounds of wavenumbers for initial perturbations.
            int m_min = m_min_candidates[std::stoi(m_idx_str)];
            int m_max = m_max_candidates[std::stoi(m_idx_str)];
            
            // Characteristic length of the initial interface thickness.
            const double epsilon_i = 0.001;
            
            // species 0: SF6.
            // species 1: air.
            const double gamma_0 = 1.09312;
            const double gamma_1 = 1.39909;
            
            const double c_p_SF6 = 668.286;
            const double c_p_air = 1040.50;
            
            const double c_v_SF6 = 611.359;
            const double c_v_air = 743.697;
            
            NULL_USE(gamma_1);
            
            // Unshocked SF6.
            const double rho_unshocked = 5.97286552525647;
            const double u_unshocked   = 0.0;
            const double v_unshocked   = 0.0;
            const double p_unshocked   = 101325.0;
            
            // Shocked SF6.
            const double rho_shocked = 11.9708247309869;
            const double u_shocked   = 98.9344103891513;
            const double v_shocked   = 0.0;
            const double p_shocked   = 218005.430874;
            
            // Air.
            const double rho_air = 1.14560096494103;
            const double u_air   = 0.0;
            const double v_air   = 0.0;
            const double p_air   = 101325.0;
            
            // Shock hits the interface after 0.05 ms.
            const double L_x_shock = 0.190127254739019;
            const double L_x_interface = 0.2;
            
            for (int j = 0; j < patch_dims[1]; j++)
            {
                for (int i = 0; i < patch_dims[0]; i++)
                {
                    // Compute the linear index.
                    int idx_cell = i + j*patch_dims[0];
                    
                    // Compute the coordinates.
                    double x[2];
                    x[0] = patch_xlo[0] + (i + 0.5)*dx[0];
                    x[1] = patch_xlo[1] + (j + 0.5)*dx[1];
                    
                    double S = 0.0;
                    for (int m = m_min; m <= m_max; m++)
                    {
                        S += A*cos(2.0*M_PI*m/0.025*x[1] + phase_shifts[30 - m]);
                    }
                    
                    if (x[0] < L_x_shock)
                    {
                        rho_Y_1[idx_cell] = rho_shocked;
                        rho_Y_2[idx_cell] = 0.0;
                        rho_u[idx_cell]   = rho_shocked*u_shocked;
                        rho_v[idx_cell]   = rho_shocked*v_shocked;
                        E[idx_cell]       = p_shocked/(gamma_0 - 1.0) +
                            0.5*rho_shocked*(u_shocked*u_shocked + v_shocked*v_shocked);
                    }
                    else
                    {
                        const double f_sm = 0.5*(1.0 + erf((x[0] - (L_x_interface + S))/epsilon_i));
                        
                        // Smooth the primitive variables.
                        const double rho_Y_1_i = rho_unshocked*(1.0 - f_sm);
                        const double rho_Y_2_i = rho_air*f_sm;
                        const double u_i = u_unshocked*(1.0 - f_sm) + u_air*f_sm;
                        const double v_i = v_unshocked*(1.0 - f_sm) + v_air*f_sm;
                        const double p_i = p_unshocked*(1.0 - f_sm) + p_air*f_sm;
                        
                        const double rho_i = rho_Y_1_i + rho_Y_2_i;
                        const double Y_0_i = rho_Y_1_i/rho_i;
                        const double Y_1_i = 1.0 - Y_0_i;
                        
                        const double gamma_i = (Y_0_i*c_p_SF6 + Y_1_i*c_p_air)/
                            (Y_0_i*c_v_SF6 + Y_1_i*c_v_air);
                        
                        rho_Y_1[idx_cell] = rho_Y_1_i;
                        rho_Y_2[idx_cell] = rho_Y_2_i;
                        rho_u[idx_cell]   = rho_i*u_i;
                        rho_v[idx_cell]   = rho_i*v_i;
                        E[idx_cell]       = p_i/(gamma_i - 1.0) + 0.5*rho_i*(u_i*u_i + v_i*v_i);
                    }
                }
            }
        }
        else
        {
            TBOX_ERROR(d_object_name
                << ": "
                << "Cannot initialize data for unknown problem with name = '"
                << d_project_name
                << "'."
                << std::endl);
        }
    }
}
