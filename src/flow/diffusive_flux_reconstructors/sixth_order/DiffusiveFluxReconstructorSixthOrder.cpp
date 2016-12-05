#include "flow/diffusive_flux_reconstructors/sixth_order/DiffusiveFluxReconstructorSixthOrder.hpp"

#include "SAMRAI/geom/CartesianPatchGeometry.h"

#include <map>

DiffusiveFluxReconstructorSixthOrder::DiffusiveFluxReconstructorSixthOrder(
    const std::string& object_name,
    const tbox::Dimension& dim,
    const boost::shared_ptr<geom::CartesianGridGeometry>& grid_geometry,
    const int& num_eqn,
    const int& num_species,
    const boost::shared_ptr<FlowModel>& flow_model,
    const boost::shared_ptr<tbox::Database>& diffusive_flux_reconstructor_db):
        DiffusiveFluxReconstructor(
            object_name,
            dim,
            grid_geometry,
            num_eqn,
            num_species,
            flow_model,
            diffusive_flux_reconstructor_db)
{
    d_num_diff_ghosts = hier::IntVector::getOne(d_dim)*6;
}


/*
 * Print all characteristics of the diffusive flux reconstruction class.
 */
void
DiffusiveFluxReconstructorSixthOrder::printClassData(
    std::ostream& os) const
{
    os << "\nPrint DiffusiveFluxReconstructorSixthOrder object..."
       << std::endl;
    
    os << std::endl;
    
    os << "DiffusiveFluxReconstructorSixthOrder: this = "
       << (DiffusiveFluxReconstructorSixthOrder *)this
       << std::endl;
    os << "d_object_name = "
       << d_object_name
       << std::endl;
}


/*
 * Put the characteristics of the diffusive flux reconstruction class
 * into the restart database.
 */
void
DiffusiveFluxReconstructorSixthOrder::putToRestart(
   const boost::shared_ptr<tbox::Database>& restart_db) const
{
    restart_db->putString("d_diffusive_flux_reconstructor", "SIXTH_ORDER");
}


/*
 * Compute the diffusive fluxes.
 */
void
DiffusiveFluxReconstructorSixthOrder::computeDiffusiveFluxes(
    hier::Patch& patch,
    const double time,
    const double dt,
    const int RK_step_number,
    const boost::shared_ptr<pdat::FaceVariable<double> >& variable_diffusive_flux,
    const boost::shared_ptr<hier::VariableContext>& data_context)
{
    NULL_USE(time);
    NULL_USE(RK_step_number);
    
    // Get the dimensions of box that covers the interior of patch.
    hier::Box interior_box = patch.getBox();
    const hier::IntVector interior_dims = interior_box.numberCells();
    
    // Get the dimensions of box that covers interior of patch plus
    // diffusive ghost cells.
    hier::Box diff_ghost_box = interior_box;
    diff_ghost_box.grow(d_num_diff_ghosts);
    const hier::IntVector diff_ghostcell_dims = diff_ghost_box.numberCells();
    
    // Get the grid spacing.
    const boost::shared_ptr<geom::CartesianPatchGeometry> patch_geom(
        BOOST_CAST<geom::CartesianPatchGeometry, hier::PatchGeometry>(
            patch.getPatchGeometry()));
    
    // Get the grid spacing.
    const double* const dx = patch_geom->getDx();
    
    // Get the face data of diffusive flux.
    boost::shared_ptr<pdat::FaceData<double> > diffusive_flux(
        BOOST_CAST<pdat::FaceData<double>, hier::PatchData>(
            patch.getPatchData(variable_diffusive_flux, data_context)));
    
    // Initialize the data of diffusive flux to zero.
    diffusive_flux->fillAll(0.0);
    
#ifdef DEBUG_CHECK_DEV_ASSERTIONS
    TBOX_ASSERT(diffusive_flux);
    TBOX_ASSERT(diffusive_flux->getGhostCellWidth() == hier::IntVector::getZero(d_dim));
#endif
    
    if (d_dim == tbox::Dimension(1))
    {
        /*
         * Register the patch and derived cell variables in the flow model and compute the corresponding cell data.
         */
        
        d_flow_model->registerPatchWithDataContext(patch, data_context);
        
        d_flow_model->registerDiffusiveFlux(d_num_diff_ghosts);
        
        d_flow_model->computeGlobalDerivedCellData();
        
        /*
         * Delcare containers for computing fluxes in different directions.
         */
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_var_data_x;
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > diffusivities_data_x;
        
        std::vector<std::vector<int> > derivative_var_component_idx_x;
        
        std::vector<std::vector<int> > diffusivities_component_idx_x;
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_x;
        
        std::map<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_x_computed;
        
        /*
         * (1) Compute the flux in the x-direction.
         */
        
        // Get the variables for the derivatives in the diffusive flux.
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_x,
            derivative_var_component_idx_x,
            DIRECTION::X_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        // Get the diffusivities in the diffusive flux.
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_x,
            diffusivities_component_idx_x,
            DIRECTION::X_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        TBOX_ASSERT(static_cast<int>(derivative_var_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_x.size()) == d_num_eqn);
        
        /*
         * Compute the derivatives in x-direction for diffusive flux in x-direction.
         */
        
        derivative_x.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_x[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_x[ei].size()));
            
            derivative_x[ei].reserve(static_cast<int>(derivative_var_data_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_x[ei][vi];
                
                if (derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))
                    == derivative_x_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_x[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudx = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_x[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_x[ei][vi]->getGhostBox().numberCells();
                    
                    for (int i = -3; i < interior_dims[0] + 3; i++)
                    {
                        // Compute the linear indices.
                        const int idx = i + d_num_diff_ghosts[0];
                        
                        const int idx_var_LLL = i - 3 + num_subghosts_variable[0];
                        const int idx_var_LL  = i - 2 + num_subghosts_variable[0];
                        const int idx_var_L   = i - 1 + num_subghosts_variable[0];
                        const int idx_var_R   = i + 1 + num_subghosts_variable[0];
                        const int idx_var_RR  = i + 2 + num_subghosts_variable[0];
                        const int idx_var_RRR = i + 3 + num_subghosts_variable[0];
                        
                        dudx[idx] = (-1.0/60.0*u[idx_var_LLL] + 3.0/20.0*u[idx_var_LL] - 3.0/4.0*u[idx_var_L]
                                     + 3.0/4.0*u[idx_var_R] - 3.0/20.0*u[idx_var_RR] + 1.0/60.0*u[idx_var_RRR])/
                                        dx[0];
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_x_computed.insert(derivative_pair);
                }
                
                derivative_x[ei].push_back(
                    derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Reconstruct the flux in x-direction.
         */
        
        for (int ei = 0; ei < d_num_eqn; ei++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_x[ei].size()) ==
                        static_cast<int>(diffusivities_data_x[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_x[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_x[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_x[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudx = derivative_x[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_x[ei][vi]->getGhostBox().numberCells();
                
                for (int i = 0; i < interior_dims[0] + 1; i++)
                {
                    // Compute the linear indices.
                    const int idx_face_x = i;
                    const int idx_diffusivity = i + num_subghosts_diffusivity[0];
                    const int idx_node_LLL = i - 3 + d_num_diff_ghosts[0];
                    const int idx_node_LL  = i - 2 + d_num_diff_ghosts[0];
                    const int idx_node_L   = i - 1 + d_num_diff_ghosts[0];
                    const int idx_node_R   = i + d_num_diff_ghosts[0];
                    const int idx_node_RR  = i + 1 + d_num_diff_ghosts[0];
                    const int idx_node_RRR = i + 2 + d_num_diff_ghosts[0];
                    
                    diffusive_flux->getPointer(0, ei)[idx_face_x] += dt*mu[idx_diffusivity]*(
                        1.0/60*(dudx[idx_node_LLL] + dudx[idx_node_RRR]) - 2.0/15.0*(dudx[idx_node_LL] + dudx[idx_node_RR])
                        + 37.0/60.0*(dudx[idx_node_L] + dudx[idx_node_R]));
                }
            }
        }
        
        derivative_var_data_x.clear();
        diffusivities_data_x.clear();
        derivative_var_component_idx_x.clear();
        diffusivities_component_idx_x.clear();
        derivative_x.clear();
        
        /*
         * Unregister the patch and data of all registered derived cell variables in the flow model.
         */
        
        d_flow_model->unregisterPatch();
        
    } // if (d_dim == tbox::Dimension(1))
    else if (d_dim == tbox::Dimension(2))
    {
        /*
         * Register the patch and derived cell variables in the flow model and compute the corresponding cell data.
         */
        
        d_flow_model->registerPatchWithDataContext(patch, data_context);
        
        d_flow_model->registerDiffusiveFlux(d_num_diff_ghosts);
        
        d_flow_model->computeGlobalDerivedCellData();
        
        /*
         * Delcare containers for computing fluxes in different directions.
         */
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_var_data_x;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_var_data_y;
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > diffusivities_data_x;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > diffusivities_data_y;
        
        std::vector<std::vector<int> > derivative_var_component_idx_x;
        std::vector<std::vector<int> > derivative_var_component_idx_y;
        
        std::vector<std::vector<int> > diffusivities_component_idx_x;
        std::vector<std::vector<int> > diffusivities_component_idx_y;
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_x;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_y;
        
        std::map<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_x_computed;
        std::map<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_y_computed;
        
        /*
         * (1) Compute the flux in the x-direction.
         */
        
        // Get the variables for the derivatives in the diffusive flux.
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_x,
            derivative_var_component_idx_x,
            DIRECTION::X_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_y,
            derivative_var_component_idx_y,
            DIRECTION::X_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        // Get the diffusivities in the diffusive flux.
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_x,
            diffusivities_component_idx_x,
            DIRECTION::X_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_y,
            diffusivities_component_idx_y,
            DIRECTION::X_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        TBOX_ASSERT(static_cast<int>(derivative_var_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_y.size()) == d_num_eqn);
        
        /*
         * Compute the derivatives in x-direction for diffusive flux in x-direction.
         */
        
        derivative_x.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_x[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_x[ei].size()));
            
            derivative_x[ei].reserve(static_cast<int>(derivative_var_data_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_x[ei][vi];
                
                if (derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))
                    == derivative_x_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_x[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudx = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_x[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_x[ei][vi]->getGhostBox().numberCells();
                    
                    for (int j = -3; j < interior_dims[1] + 3; j++)
                    {
                        for (int i = -3; i < interior_dims[0] + 3; i++)
                        {
                            // Compute the linear indices.
                            const int idx = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                            
                            const int idx_var_LLL = (i - 3 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_LL = (i - 2 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_L = (i - 1 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_R = (i + 1 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_RR = (i + 2 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_RRR = (i + 3 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            dudx[idx] = (-1.0/60.0*u[idx_var_LLL] + 3.0/20.0*u[idx_var_LL] - 3.0/4.0*u[idx_var_L]
                                         + 3.0/4.0*u[idx_var_R] - 3.0/20.0*u[idx_var_RR] + 1.0/60.0*u[idx_var_RRR])/
                                            dx[0];
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_x_computed.insert(derivative_pair);
                }
                
                derivative_x[ei].push_back(
                    derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in y-direction for diffusive flux in x-direction.
         */
        
        derivative_y.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_y[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_y[ei].size()));
            
            derivative_y[ei].reserve(static_cast<int>(derivative_var_data_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_y[ei][vi];
                
                if (derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))
                    == derivative_y_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_y[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudy = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_y[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_y[ei][vi]->getGhostBox().numberCells();
                    
                    for (int j = -3; j < interior_dims[1] + 3; j++)
                    {
                        for (int i = -3; i < interior_dims[0] + 3; i++)
                        {
                            // Compute the linear indices.
                            const int idx = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                            
                            const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                (j - 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                (j - 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_B = (i + num_subghosts_variable[0]) +
                                (j - 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_T = (i + num_subghosts_variable[0]) +
                                (j + 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_TT = (i + num_subghosts_variable[0]) +
                                (j + 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_TTT = (i + num_subghosts_variable[0]) +
                                (j + 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            dudy[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                         + 3.0/4.0*u[idx_var_T] - 3.0/20.0*u[idx_var_TT] + 1.0/60.0*u[idx_var_TTT])/
                                            dx[1];
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_y_computed.insert(derivative_pair);
                }
                
                derivative_y[ei].push_back(
                    derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Reconstruct the flux in x-direction.
         */
        
        for (int ei = 0; ei < d_num_eqn; ei++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_x[ei].size()) ==
                        static_cast<int>(diffusivities_data_x[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_x[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_x[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_x[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudx = derivative_x[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_x[ei][vi]->getGhostBox().numberCells();
                
                for (int j = 0; j < interior_dims[1]; j++)
                {
                    for (int i = 0; i < interior_dims[0] + 1; i++)
                    {
                        // Compute the linear indices.
                        const int idx_face_x = i +
                            j*(interior_dims[0] + 1);
                        
                        const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                            (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0];
                        
                        const int idx_node_LLL = (i - 3 + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_LL = (i - 2 + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_L = (i - 1 + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_R = (i + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_RR = (i + 1 + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_RRR = (i + 2 + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        diffusive_flux->getPointer(0, ei)[idx_face_x] += dt*mu[idx_diffusivity]*(
                            1.0/60*(dudx[idx_node_LLL] + dudx[idx_node_RRR])
                            - 2.0/15.0*(dudx[idx_node_LL] + dudx[idx_node_RR])
                            + 37.0/60.0*(dudx[idx_node_L] + dudx[idx_node_R]));
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_y[ei].size()) ==
                        static_cast<int>(diffusivities_data_y[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_y[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_y[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_y[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudy = derivative_y[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_y[ei][vi]->getGhostBox().numberCells();
                
                for (int j = 0; j < interior_dims[1]; j++)
                {
                    for (int i = 0; i < interior_dims[0] + 1; i++)
                    {
                        // Compute the linear indices.
                        const int idx_face_x = i +
                            j*(interior_dims[0] + 1);
                        
                        const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                            (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0];
                        
                        const int idx_node_LLL = (i - 3 + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_LL  = i - 2 + d_num_diff_ghosts[0] +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_L   = i - 1 + d_num_diff_ghosts[0] +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_R   = i + d_num_diff_ghosts[0] +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_RR  = i + 1 + d_num_diff_ghosts[0] +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_RRR = i + 2 + d_num_diff_ghosts[0] +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        diffusive_flux->getPointer(0, ei)[idx_face_x] += dt*mu[idx_diffusivity]*(
                            1.0/60*(dudy[idx_node_LLL] + dudy[idx_node_RRR])
                            - 2.0/15.0*(dudy[idx_node_LL] + dudy[idx_node_RR])
                            + 37.0/60.0*(dudy[idx_node_L] + dudy[idx_node_R]));
                    }
                }
            }
        }
        
        derivative_var_data_x.clear();
        derivative_var_data_y.clear();
        
        diffusivities_data_x.clear();
        diffusivities_data_y.clear();
        
        derivative_var_component_idx_x.clear();
        derivative_var_component_idx_y.clear();
        
        diffusivities_component_idx_x.clear();
        diffusivities_component_idx_y.clear();
        
        derivative_x.clear();
        derivative_y.clear();
        
        /*
         * (2) Compute the flux in the y-direction.
         */
        
        // Get the variables for the derivatives in the diffusive flux.
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_x,
            derivative_var_component_idx_x,
            DIRECTION::Y_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_y,
            derivative_var_component_idx_y,
            DIRECTION::Y_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        // Get the diffusivities in the diffusive flux.
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_x,
            diffusivities_component_idx_x,
            DIRECTION::Y_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_y,
            diffusivities_component_idx_y,
            DIRECTION::Y_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        TBOX_ASSERT(static_cast<int>(derivative_var_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_y.size()) == d_num_eqn);
        
        /*
         * Compute the derivatives in x-direction for diffusive flux in y-direction.
         */
        
        derivative_x.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_x[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_x[ei].size()));
            
            derivative_x[ei].reserve(static_cast<int>(derivative_var_data_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_x[ei][vi];
                
                if (derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))
                    == derivative_x_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_x[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudx = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_x[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_x[ei][vi]->getGhostBox().numberCells();
                    
                    for (int j = -3; j < interior_dims[1] + 3; j++)
                    {
                        for (int i = -3; i < interior_dims[0] + 3; i++)
                        {
                            // Compute the linear indices.
                            const int idx = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                            
                            const int idx_var_LLL = (i - 3 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_LL = (i - 2 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_L = (i - 1 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_R = (i + 1 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_RR = (i + 2 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_RRR = (i + 3 + num_subghosts_variable[0]) +
                                (j + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            dudx[idx] = (-1.0/60.0*u[idx_var_LLL] + 3.0/20.0*u[idx_var_LL] - 3.0/4.0*u[idx_var_L]
                                         + 3.0/4.0*u[idx_var_R] - 3.0/20.0*u[idx_var_RR] + 1.0/60.0*u[idx_var_RRR])/
                                            dx[0];
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_x_computed.insert(derivative_pair);
                }
                
                derivative_x[ei].push_back(
                    derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in y-direction for diffusive flux in y-direction.
         */
        
        derivative_y.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_y[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_y[ei].size()));
            
            derivative_y[ei].reserve(static_cast<int>(derivative_var_data_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_y[ei][vi];
                
                if (derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))
                    == derivative_y_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_y[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudy = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_y[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_y[ei][vi]->getGhostBox().numberCells();
                    
                    for (int j = -3; j < interior_dims[1] + 3; j++)
                    {
                        for (int i = -3; i < interior_dims[0] + 3; i++)
                        {
                            // Compute the linear indices.
                            const int idx = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                            
                            const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                (j - 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                (j - 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_B = (i + num_subghosts_variable[0]) +
                                (j - 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_T = (i + num_subghosts_variable[0]) +
                                (j + 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_TT = (i + num_subghosts_variable[0]) +
                                (j + 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            const int idx_var_TTT = (i + num_subghosts_variable[0]) +
                                (j + 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0];
                            
                            dudy[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                         + 3.0/4.0*u[idx_var_T] - 3.0/20.0*u[idx_var_TT] + 1.0/60.0*u[idx_var_TTT])/
                                            dx[1];
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_y_computed.insert(derivative_pair);
                }
                
                derivative_y[ei].push_back(
                    derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Reconstruct the flux in y-direction.
         */
        
        for (int ei = 0; ei < d_num_eqn; ei++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_x[ei].size()) ==
                        static_cast<int>(diffusivities_data_x[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_x[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_x[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_x[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudx = derivative_x[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_x[ei][vi]->getGhostBox().numberCells();
                
                for (int i = 0; i < interior_dims[0]; i++)
                {
                    for (int j = 0; j < interior_dims[1] + 1; j++)
                    {
                        // Compute the linear indices.
                        const int idx_face_y = j +
                            i*(interior_dims[1] + 1);
                        
                        const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                            (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0];
                        
                        const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                            (j - 3 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                            (j - 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                            (j - 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_T = (i + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_TT = (i + d_num_diff_ghosts[0]) +
                            (j + 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_TTT = (i + d_num_diff_ghosts[0]) +
                            (j + 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        diffusive_flux->getPointer(1, ei)[idx_face_y] += dt*mu[idx_diffusivity]*(
                            1.0/60*(dudx[idx_node_BBB] + dudx[idx_node_TTT])
                            - 2.0/15.0*(dudx[idx_node_BB] + dudx[idx_node_TT])
                            + 37.0/60.0*(dudx[idx_node_B] + dudx[idx_node_T]));
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_y[ei].size()) ==
                        static_cast<int>(diffusivities_data_y[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_y[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_y[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_y[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudy = derivative_y[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_y[ei][vi]->getGhostBox().numberCells();
                
                for (int i = 0; i < interior_dims[0]; i++)
                {
                    for (int j = 0; j < interior_dims[1] + 1; j++)
                    {
                        // Compute the linear indices.
                        const int idx_face_y = j +
                            i*(interior_dims[1] + 1);
                        
                        const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                            (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0];
                        
                        const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                            (j - 3 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                            (j - 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                            (j - 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_T = (i + d_num_diff_ghosts[0]) +
                            (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_TT = (i + d_num_diff_ghosts[0]) +
                            (j + 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        const int idx_node_TTT = (i + d_num_diff_ghosts[0]) +
                            (j + 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0];
                        
                        diffusive_flux->getPointer(1, ei)[idx_face_y] += dt*mu[idx_diffusivity]*(
                            1.0/60*(dudy[idx_node_BBB] + dudy[idx_node_TTT])
                            - 2.0/15.0*(dudy[idx_node_BB] + dudy[idx_node_TT])
                            + 37.0/60.0*(dudy[idx_node_B] + dudy[idx_node_T]));
                    }
                }
            }
        }
        
        derivative_var_data_x.clear();
        derivative_var_data_y.clear();
        
        diffusivities_data_x.clear();
        diffusivities_data_y.clear();
        
        derivative_var_component_idx_x.clear();
        derivative_var_component_idx_y.clear();
        
        diffusivities_component_idx_x.clear();
        diffusivities_component_idx_y.clear();
        
        derivative_x.clear();
        derivative_y.clear();
        
        /*
         * Unregister the patch and data of all registered derived cell variables in the flow model.
         */
        
        d_flow_model->unregisterPatch();
        
    } // if (d_dim == tbox::Dimension(2))
    else if (d_dim == tbox::Dimension(3))
    {
        /*
         * Register the patch and derived cell variables in the flow model and compute the corresponding cell data.
         */
        
        d_flow_model->registerPatchWithDataContext(patch, data_context);
        
        d_flow_model->registerDiffusiveFlux(d_num_diff_ghosts);
        
        d_flow_model->computeGlobalDerivedCellData();
        
        /*
         * Delcare containers for computing fluxes in different directions.
         */
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_var_data_x;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_var_data_y;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_var_data_z;
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > diffusivities_data_x;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > diffusivities_data_y;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > diffusivities_data_z;
        
        std::vector<std::vector<int> > derivative_var_component_idx_x;
        std::vector<std::vector<int> > derivative_var_component_idx_y;
        std::vector<std::vector<int> > derivative_var_component_idx_z;
        
        std::vector<std::vector<int> > diffusivities_component_idx_x;
        std::vector<std::vector<int> > diffusivities_component_idx_y;
        std::vector<std::vector<int> > diffusivities_component_idx_z;
        
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_x;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_y;
        std::vector<std::vector<boost::shared_ptr<pdat::CellData<double> > > > derivative_z;
        
        std::map<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_x_computed;
        std::map<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_y_computed;
        std::map<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_z_computed;
        
        /*
         * (1) Compute the flux in the x-direction.
         */
        
        // Get the variables for the derivatives in the diffusive flux.
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_x,
            derivative_var_component_idx_x,
            DIRECTION::X_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_y,
            derivative_var_component_idx_y,
            DIRECTION::X_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_z,
            derivative_var_component_idx_z,
            DIRECTION::X_DIRECTION,
            DIRECTION::Z_DIRECTION);
        
        // Get the diffusivities in the diffusive flux.
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_x,
            diffusivities_component_idx_x,
            DIRECTION::X_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_y,
            diffusivities_component_idx_y,
            DIRECTION::X_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_z,
            diffusivities_component_idx_z,
            DIRECTION::X_DIRECTION,
            DIRECTION::Z_DIRECTION);
        
        TBOX_ASSERT(static_cast<int>(derivative_var_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_z.size()) == d_num_eqn);
        
        /*
         * Compute the derivatives in x-direction for diffusive flux in x-direction.
         */
        
        derivative_x.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_x[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_x[ei].size()));
            
            derivative_x[ei].reserve(static_cast<int>(derivative_var_data_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_x[ei][vi];
                
                if (derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))
                    == derivative_x_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_x[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudx = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_x[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_x[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_LLL = (i - 3 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_LL = (i - 2 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_L = (i - 1 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_R = (i + 1 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_RR = (i + 2 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_RRR = (i + 3 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudx[idx] = (-1.0/60.0*u[idx_var_LLL] + 3.0/20.0*u[idx_var_LL] - 3.0/4.0*u[idx_var_L]
                                             + 3.0/4.0*u[idx_var_R] - 3.0/20.0*u[idx_var_RR] + 1.0/60.0*u[idx_var_RRR])/
                                                dx[0];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_x_computed.insert(derivative_pair);
                }
                
                derivative_x[ei].push_back(
                    derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in y-direction for diffusive flux in x-direction.
         */
        
        derivative_y.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_y[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_y[ei].size()));
            
            derivative_y[ei].reserve(static_cast<int>(derivative_var_data_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_y[ei][vi];
                
                if (derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))
                    == derivative_y_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_y[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudy = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_y[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_y[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                    (j - 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                    (j - 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_B = (i + num_subghosts_variable[0]) +
                                    (j - 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_T = (i + num_subghosts_variable[0]) +
                                    (j + 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_TT = (i + num_subghosts_variable[0]) +
                                    (j + 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_TTT = (i + num_subghosts_variable[0]) +
                                    (j + 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudy[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                             + 3.0/4.0*u[idx_var_T] - 3.0/20.0*u[idx_var_TT] + 1.0/60.0*u[idx_var_TTT])/
                                                dx[1];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_y_computed.insert(derivative_pair);
                }
                
                derivative_y[ei].push_back(
                    derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in z-direction for diffusive flux in x-direction.
         */
        
        derivative_z.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_z[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_z[ei].size()));
            
            derivative_z[ei].reserve(static_cast<int>(derivative_var_data_z[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_z[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_z[ei][vi];
                
                if (derivative_z_computed.find(derivative_var_data_z[ei][vi]->getPointer(u_idx))
                    == derivative_z_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_z[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudz = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_z[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_z[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 3 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 2 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_B = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 1 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_F = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 1 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_FF = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 2 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_FFF = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 3 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudz[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                             + 3.0/4.0*u[idx_var_F] - 3.0/20.0*u[idx_var_FF] + 1.0/60.0*u[idx_var_FFF])/
                                                dx[2];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_z_computed.insert(derivative_pair);
                }
                
                derivative_z[ei].push_back(
                    derivative_z_computed.find(derivative_var_data_z[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Reconstruct the flux in x-direction.
         */
        
        for (int ei = 0; ei < d_num_eqn; ei++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_x[ei].size()) ==
                        static_cast<int>(diffusivities_data_x[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_x[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_x[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_x[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudx = derivative_x[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_x[ei][vi]->getGhostBox().numberCells();
                
                for (int k = 0; k < interior_dims[2]; k++)
                {
                    for (int j = 0; j < interior_dims[1]; j++)
                    {
                        for (int i = 0; i < interior_dims[0] + 1; i++)
                        {
                            // Compute the linear indices.
                            const int idx_face_x = i +
                                j*(interior_dims[0] + 1) +
                                k*(interior_dims[0] + 1)*interior_dims[1];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_LLL = (i - 3 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_LL = (i - 2 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_L = (i - 1 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_R = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_RR = (i + 1 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_RRR = (i + 2 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(0, ei)[idx_face_x] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudx[idx_node_LLL] + dudx[idx_node_RRR])
                                - 2.0/15.0*(dudx[idx_node_LL] + dudx[idx_node_RR])
                                + 37.0/60.0*(dudx[idx_node_L] + dudx[idx_node_R]));
                        }
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_y[ei].size()) ==
                        static_cast<int>(diffusivities_data_y[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_y[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_y[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_y[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudy = derivative_y[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_y[ei][vi]->getGhostBox().numberCells();
                
                for (int k = 0; k < interior_dims[2]; k++)
                {
                    for (int j = 0; j < interior_dims[1]; j++)
                    {
                        for (int i = 0; i < interior_dims[0] + 1; i++)
                        {
                            // Compute the linear indices.
                            const int idx_face_x = i +
                                j*(interior_dims[0] + 1) +
                                k*(interior_dims[0] + 1)*interior_dims[1];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_LLL = (i - 3 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_LL = (i - 2 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_L = (i - 1 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_R = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_RR = (i + 1 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_RRR = (i + 2 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(0, ei)[idx_face_x] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudy[idx_node_LLL] + dudy[idx_node_RRR])
                                - 2.0/15.0*(dudy[idx_node_LL] + dudy[idx_node_RR])
                                + 37.0/60.0*(dudy[idx_node_L] + dudy[idx_node_R]));
                        }
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_z[ei].size()) ==
                        static_cast<int>(diffusivities_data_z[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_z[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_z[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_z[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_z[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_z[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudz = derivative_z[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_z[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_z[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_z[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_z[ei][vi]->getGhostBox().numberCells();
                
                for (int k = 0; k < interior_dims[2]; k++)
                {
                    for (int j = 0; j < interior_dims[1]; j++)
                    {
                        for (int i = 0; i < interior_dims[0] + 1; i++)
                        {
                            // Compute the linear indices.
                            const int idx_face_x = i +
                                j*(interior_dims[0] + 1) +
                                k*(interior_dims[0] + 1)*interior_dims[1];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_LLL = (i - 3 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_LL = (i - 2 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_L = (i - 1 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_R = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_RR = (i + 1 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_RRR = (i + 2 + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(0, ei)[idx_face_x] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudz[idx_node_LLL] + dudz[idx_node_RRR])
                                - 2.0/15.0*(dudz[idx_node_LL] + dudz[idx_node_RR])
                                + 37.0/60.0*(dudz[idx_node_L] + dudz[idx_node_R]));
                        }
                    }
                }
            }
        }
        
        derivative_var_data_x.clear();
        derivative_var_data_y.clear();
        derivative_var_data_z.clear();
        
        diffusivities_data_x.clear();
        diffusivities_data_y.clear();
        diffusivities_data_z.clear();
        
        derivative_var_component_idx_x.clear();
        derivative_var_component_idx_y.clear();
        derivative_var_component_idx_z.clear();
        
        diffusivities_component_idx_x.clear();
        diffusivities_component_idx_y.clear();
        diffusivities_component_idx_z.clear();
        
        derivative_x.clear();
        derivative_y.clear();
        derivative_z.clear();
        
        /*
         * (2) Compute the flux in the y-direction.
         */
        
        // Get the variables for the derivatives in the diffusive flux.
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_x,
            derivative_var_component_idx_x,
            DIRECTION::Y_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_y,
            derivative_var_component_idx_y,
            DIRECTION::Y_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_z,
            derivative_var_component_idx_z,
            DIRECTION::Y_DIRECTION,
            DIRECTION::Z_DIRECTION);
        
        // Get the diffusivities in the diffusive flux.
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_x,
            diffusivities_component_idx_x,
            DIRECTION::Y_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_y,
            diffusivities_component_idx_y,
            DIRECTION::Y_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_z,
            diffusivities_component_idx_z,
            DIRECTION::Y_DIRECTION,
            DIRECTION::Z_DIRECTION);
        
        TBOX_ASSERT(static_cast<int>(derivative_var_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_z.size()) == d_num_eqn);
        
        /*
         * Compute the derivatives in x-direction for diffusive flux in y-direction.
         */
        
        derivative_x.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_x[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_x[ei].size()));
            
            derivative_x[ei].reserve(static_cast<int>(derivative_var_data_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_x[ei][vi];
                
                if (derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))
                    == derivative_x_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_x[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudx = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_x[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_x[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_LLL = (i - 3 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_LL = (i - 2 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_L = (i - 1 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_R = (i + 1 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_RR = (i + 2 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_RRR = (i + 3 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudx[idx] = (-1.0/60.0*u[idx_var_LLL] + 3.0/20.0*u[idx_var_LL] - 3.0/4.0*u[idx_var_L]
                                             + 3.0/4.0*u[idx_var_R] - 3.0/20.0*u[idx_var_RR] + 1.0/60.0*u[idx_var_RRR])/
                                                dx[0];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_x_computed.insert(derivative_pair);
                }
                
                derivative_x[ei].push_back(
                    derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in y-direction for diffusive flux in y-direction.
         */
        
        derivative_y.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_y[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_y[ei].size()));
            
            derivative_y[ei].reserve(static_cast<int>(derivative_var_data_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_y[ei][vi];
                
                if (derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))
                    == derivative_y_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_y[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudy = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_y[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_y[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                    (j - 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                    (j - 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_B = (i + num_subghosts_variable[0]) +
                                    (j - 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_T = (i + num_subghosts_variable[0]) +
                                    (j + 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_TT = (i + num_subghosts_variable[0]) +
                                    (j + 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_TTT = (i + num_subghosts_variable[0]) +
                                    (j + 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudy[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                             + 3.0/4.0*u[idx_var_T] - 3.0/20.0*u[idx_var_TT] + 1.0/60.0*u[idx_var_TTT])/
                                                dx[1];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_y_computed.insert(derivative_pair);
                }
                
                derivative_y[ei].push_back(
                    derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in z-direction for diffusive flux in y-direction.
         */
        
        derivative_z.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_z[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_z[ei].size()));
            
            derivative_z[ei].reserve(static_cast<int>(derivative_var_data_z[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_z[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_z[ei][vi];
                
                if (derivative_z_computed.find(derivative_var_data_z[ei][vi]->getPointer(u_idx))
                    == derivative_z_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_z[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudz = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_z[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_z[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 3 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 2 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_B = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 1 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_F = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 1 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_FF = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 2 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_FFF = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 3 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudz[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                             + 3.0/4.0*u[idx_var_F] - 3.0/20.0*u[idx_var_FF] + 1.0/60.0*u[idx_var_FFF])/
                                                dx[2];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_z_computed.insert(derivative_pair);
                }
                
                derivative_z[ei].push_back(
                    derivative_z_computed.find(derivative_var_data_z[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Reconstruct the flux in y-direction.
         */
        
        for (int ei = 0; ei < d_num_eqn; ei++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_x[ei].size()) ==
                        static_cast<int>(diffusivities_data_x[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_x[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_x[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_x[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudx = derivative_x[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_x[ei][vi]->getGhostBox().numberCells();
                
                for (int i = 0; i < interior_dims[0]; i++)
                {
                    for (int k = 0; k < interior_dims[2]; k++)
                    {
                        for (int j = 0; j < interior_dims[1] + 1; j++)
                        {
                            // Compute the linear indices.
                            const int idx_face_y = j +
                                k*(interior_dims[1] + 1) +
                                i*(interior_dims[1] + 1)*interior_dims[2];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                                (j - 3 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                                (j - 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                                (j - 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_T = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_TT = (i + d_num_diff_ghosts[0]) +
                                (j + 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_TTT = (i + d_num_diff_ghosts[0]) +
                                (j + 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(1, ei)[idx_face_y] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudx[idx_node_BBB] + dudx[idx_node_TTT])
                                - 2.0/15.0*(dudx[idx_node_BB] + dudx[idx_node_TT])
                                + 37.0/60.0*(dudx[idx_node_B] + dudx[idx_node_T]));
                        }
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_y[ei].size()) ==
                        static_cast<int>(diffusivities_data_y[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_y[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_y[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_y[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudy = derivative_y[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_y[ei][vi]->getGhostBox().numberCells();
                
                for (int i = 0; i < interior_dims[0]; i++)
                {
                    for (int k = 0; k < interior_dims[2]; k++)
                    {
                        for (int j = 0; j < interior_dims[1] + 1; j++)
                        {
                            // Compute the linear indices.
                            const int idx_face_y = j +
                                k*(interior_dims[1] + 1) +
                                i*(interior_dims[1] + 1)*interior_dims[2];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                                (j - 3 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                                (j - 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                                (j - 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_T = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_TT = (i + d_num_diff_ghosts[0]) +
                                (j + 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_TTT = (i + d_num_diff_ghosts[0]) +
                                (j + 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(1, ei)[idx_face_y] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudy[idx_node_BBB] + dudy[idx_node_TTT])
                                - 2.0/15.0*(dudy[idx_node_BB] + dudy[idx_node_TT])
                                + 37.0/60.0*(dudy[idx_node_B] + dudy[idx_node_T]));
                        }
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_z[ei].size()) ==
                        static_cast<int>(diffusivities_data_z[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_z[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_z[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_z[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_z[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_z[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudz = derivative_z[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_z[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_z[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_z[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_z[ei][vi]->getGhostBox().numberCells();
                
                for (int i = 0; i < interior_dims[0]; i++)
                {
                    for (int k = 0; k < interior_dims[2]; k++)
                    {
                        for (int j = 0; j < interior_dims[1] + 1; j++)
                        {
                            // Compute the linear indices.
                            const int idx_face_y = j +
                                k*(interior_dims[1] + 1) +
                                i*(interior_dims[1] + 1)*interior_dims[2];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                                (j - 3 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                                (j - 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                                (j - 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_T = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_TT = (i + d_num_diff_ghosts[0]) +
                                (j + 1 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_TTT = (i + d_num_diff_ghosts[0]) +
                                (j + 2 + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(1, ei)[idx_face_y] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudz[idx_node_BBB] + dudz[idx_node_TTT])
                                - 2.0/15.0*(dudz[idx_node_BB] + dudz[idx_node_TT])
                                + 37.0/60.0*(dudz[idx_node_B] + dudz[idx_node_T]));
                        }
                    }
                }
            }
        }
        
        derivative_var_data_x.clear();
        derivative_var_data_y.clear();
        derivative_var_data_z.clear();
        
        diffusivities_data_x.clear();
        diffusivities_data_y.clear();
        diffusivities_data_z.clear();
        
        derivative_var_component_idx_x.clear();
        derivative_var_component_idx_y.clear();
        derivative_var_component_idx_z.clear();
        
        diffusivities_component_idx_x.clear();
        diffusivities_component_idx_y.clear();
        diffusivities_component_idx_z.clear();
        
        derivative_x.clear();
        derivative_y.clear();
        derivative_z.clear();
        
        /*
         * (3) Compute the flux in the z-direction.
         */
        
        // Get the variables for the derivatives in the diffusive flux.
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_x,
            derivative_var_component_idx_x,
            DIRECTION::Z_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_y,
            derivative_var_component_idx_y,
            DIRECTION::Z_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        d_flow_model->getDiffusiveFluxVariablesForDerivative(
            derivative_var_data_z,
            derivative_var_component_idx_z,
            DIRECTION::Z_DIRECTION,
            DIRECTION::Z_DIRECTION);
        
        // Get the diffusivities in the diffusive flux.
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_x,
            diffusivities_component_idx_x,
            DIRECTION::Z_DIRECTION,
            DIRECTION::X_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_y,
            diffusivities_component_idx_y,
            DIRECTION::Z_DIRECTION,
            DIRECTION::Y_DIRECTION);
        
        d_flow_model->getDiffusiveFluxDiffusivities(
            diffusivities_data_z,
            diffusivities_component_idx_z,
            DIRECTION::Z_DIRECTION,
            DIRECTION::Z_DIRECTION);
        
        TBOX_ASSERT(static_cast<int>(derivative_var_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_data_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(derivative_var_component_idx_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_data_z.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_x.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_y.size()) == d_num_eqn);
        TBOX_ASSERT(static_cast<int>(diffusivities_component_idx_z.size()) == d_num_eqn);
        
        /*
         * Compute the derivatives in x-direction for diffusive flux in z-direction.
         */
        
        derivative_x.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_x[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_x[ei].size()));
            
            derivative_x[ei].reserve(static_cast<int>(derivative_var_data_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_x[ei][vi];
                
                if (derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))
                    == derivative_x_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_x[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudx = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_x[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_x[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_LLL = (i - 3 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_LL = (i - 2 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_L = (i - 1 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_R = (i + 1 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_RR = (i + 2 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_RRR = (i + 3 + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudx[idx] = (-1.0/60.0*u[idx_var_LLL] + 3.0/20.0*u[idx_var_LL] - 3.0/4.0*u[idx_var_L]
                                             + 3.0/4.0*u[idx_var_R] - 3.0/20.0*u[idx_var_RR] + 1.0/60.0*u[idx_var_RRR])/
                                                dx[0];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_x_computed.insert(derivative_pair);
                }
                
                derivative_x[ei].push_back(
                    derivative_x_computed.find(derivative_var_data_x[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in y-direction for diffusive flux in z-direction.
         */
        
        derivative_y.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_y[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_y[ei].size()));
            
            derivative_y[ei].reserve(static_cast<int>(derivative_var_data_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_y[ei][vi];
                
                if (derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))
                    == derivative_y_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_y[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudy = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_y[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_y[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                    (j - 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                    (j - 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_B = (i + num_subghosts_variable[0]) +
                                    (j - 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_T = (i + num_subghosts_variable[0]) +
                                    (j + 1 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_TT = (i + num_subghosts_variable[0]) +
                                    (j + 2 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_TTT = (i + num_subghosts_variable[0]) +
                                    (j + 3 + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudy[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                             + 3.0/4.0*u[idx_var_T] - 3.0/20.0*u[idx_var_TT] + 1.0/60.0*u[idx_var_TTT])/
                                                dx[1];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_y_computed.insert(derivative_pair);
                }
                
                derivative_y[ei].push_back(
                    derivative_y_computed.find(derivative_var_data_y[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Compute the derivatives in z-direction for diffusive flux in z-direction.
         */
        
        derivative_z.resize(d_num_eqn);
        
        for (int ei = 0; ei < d_num_eqn; ei ++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_var_data_z[ei].size()) ==
                        static_cast<int>(derivative_var_component_idx_z[ei].size()));
            
            derivative_z[ei].reserve(static_cast<int>(derivative_var_data_z[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_z[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int u_idx = derivative_var_component_idx_z[ei][vi];
                
                if (derivative_z_computed.find(derivative_var_data_z[ei][vi]->getPointer(u_idx))
                    == derivative_z_computed.end())
                {
                    // Get the pointer to variable for derivative.
                    double* u = derivative_var_data_z[ei][vi]->getPointer(u_idx);
                    
                    // Declare container to store the derivative.
                    boost::shared_ptr<pdat::CellData<double> > derivative(
                        new pdat::CellData<double>(
                            interior_box, 1, d_num_diff_ghosts));
                    
                    // Get the pointer to the derivative.
                    double* dudz = derivative->getPointer(0);
                    
                    /*
                     * Get the sub-ghost cell width and ghost box dimensions of the variable.
                     */
                    
                    hier::IntVector num_subghosts_variable =
                        derivative_var_data_z[ei][vi]->getGhostCellWidth();
                    
                    hier::IntVector subghostcell_dims_variable =
                        derivative_var_data_z[ei][vi]->getGhostBox().numberCells();
                    
                    for (int k = -3; k < interior_dims[2] + 3; k++)
                    {
                        for (int j = -3; j < interior_dims[1] + 3; j++)
                        {
                            for (int i = -3; i < interior_dims[0] + 3; i++)
                            {
                                // Compute the linear indices.
                                const int idx = (i + d_num_diff_ghosts[0]) +
                                    (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                    (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                        diff_ghostcell_dims[1];
                                
                                const int idx_var_BBB = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 3 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_BB = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 2 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_B = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k - 1 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_F = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 1 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_FF = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 2 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                const int idx_var_FFF = (i + num_subghosts_variable[0]) +
                                    (j + num_subghosts_variable[1])*subghostcell_dims_variable[0] +
                                    (k + 3 + num_subghosts_variable[2])*subghostcell_dims_variable[0]*
                                        subghostcell_dims_variable[1];
                                
                                dudz[idx] = (-1.0/60.0*u[idx_var_BBB] + 3.0/20.0*u[idx_var_BB] - 3.0/4.0*u[idx_var_B]
                                             + 3.0/4.0*u[idx_var_F] - 3.0/20.0*u[idx_var_FF] + 1.0/60.0*u[idx_var_FFF])/
                                                dx[2];
                            }
                        }
                    }
                    
                    std::pair<double*, boost::shared_ptr<pdat::CellData<double> > > derivative_pair(
                        u,
                        derivative);
                    
                    derivative_z_computed.insert(derivative_pair);
                }
                
                derivative_z[ei].push_back(
                    derivative_z_computed.find(derivative_var_data_z[ei][vi]->getPointer(u_idx))->
                        second);
            }
        }
        
        /*
         * Reconstruct the flux in z-direction.
         */
        
        for (int ei = 0; ei < d_num_eqn; ei++)
        {
            TBOX_ASSERT(static_cast<int>(derivative_x[ei].size()) ==
                        static_cast<int>(diffusivities_data_x[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_x[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_x[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_x[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_x[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_x[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudx = derivative_x[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_x[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_x[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_x[ei][vi]->getGhostBox().numberCells();
                
                for (int j = 0; j < interior_dims[1]; j++)
                {
                    for (int i = 0; i < interior_dims[0]; i++)
                    {
                        for (int k = 0; k < interior_dims[2] + 1; k++)
                        {
                            // Compute the linear indices.
                            const int idx_face_z = k +
                                i*(interior_dims[2] + 1) +
                                j*(interior_dims[2] + 1)*interior_dims[0];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 3 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 2 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 1 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_F = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_FF = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + 1 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_FFF = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + 2 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(2, ei)[idx_face_z] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudx[idx_node_BBB] + dudx[idx_node_FFF])
                                - 2.0/15.0*(dudx[idx_node_BB] + dudx[idx_node_FF])
                                + 37.0/60.0*(dudx[idx_node_B] + dudx[idx_node_F]));
                        }
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_y[ei].size()) ==
                        static_cast<int>(diffusivities_data_y[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_y[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_y[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_y[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_y[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_y[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudy = derivative_y[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_y[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_y[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_y[ei][vi]->getGhostBox().numberCells();
                
                for (int j = 0; j < interior_dims[1]; j++)
                {
                    for (int i = 0; i < interior_dims[0]; i++)
                    {
                        for (int k = 0; k < interior_dims[2] + 1; k++)
                        {
                            // Compute the linear indices.
                            const int idx_face_z = k +
                                i*(interior_dims[2] + 1) +
                                j*(interior_dims[2] + 1)*interior_dims[0];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 3 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 2 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 1 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_F = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_FF = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + 1 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_FFF = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + 2 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(2, ei)[idx_face_z] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudy[idx_node_BBB] + dudy[idx_node_FFF])
                                - 2.0/15.0*(dudy[idx_node_BB] + dudy[idx_node_FF])
                                + 37.0/60.0*(dudy[idx_node_B] + dudy[idx_node_F]));
                        }
                    }
                }
            }
            
            TBOX_ASSERT(static_cast<int>(derivative_z[ei].size()) ==
                        static_cast<int>(diffusivities_data_z[ei].size()));
            
            TBOX_ASSERT(static_cast<int>(diffusivities_data_z[ei].size()) ==
                        static_cast<int>(diffusivities_component_idx_z[ei].size()));
            
            for (int vi = 0; vi < static_cast<int>(derivative_var_data_z[ei].size()); vi++)
            {
                // Get the index of variable for derivative.
                const int mu_idx = diffusivities_component_idx_z[ei][vi];
                
                // Get the pointer to diffusivity.
                double* mu = diffusivities_data_z[ei][vi]->getPointer(mu_idx);
                
                // Get the pointer to derivative.
                double* dudz = derivative_z[ei][vi]->getPointer(0);
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the diffusivity.
                 */
                
                hier::IntVector num_subghosts_diffusivity =
                    diffusivities_data_z[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_diffusivity =
                    diffusivities_data_z[ei][vi]->getGhostBox().numberCells();
                
                /*
                 * Get the sub-ghost cell width and ghost box dimensions of the derivative.
                 */
                
                hier::IntVector num_subghosts_derivative =
                    derivative_z[ei][vi]->getGhostCellWidth();
                
                hier::IntVector subghostcell_dims_derivative =
                    derivative_z[ei][vi]->getGhostBox().numberCells();
                
                for (int j = 0; j < interior_dims[1]; j++)
                {
                    for (int i = 0; i < interior_dims[0]; i++)
                    {
                        for (int k = 0; k < interior_dims[2] + 1; k++)
                        {
                            // Compute the linear indices.
                            const int idx_face_z = k +
                                i*(interior_dims[2] + 1) +
                                j*(interior_dims[2] + 1)*interior_dims[0];
                            
                            const int idx_diffusivity = (i + num_subghosts_diffusivity[0]) +
                                (j + num_subghosts_diffusivity[1])*subghostcell_dims_diffusivity[0] +
                                (k + num_subghosts_diffusivity[2])*subghostcell_dims_diffusivity[0]*
                                    subghostcell_dims_diffusivity[1];
                            
                            const int idx_node_BBB = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 3 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_BB = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 2 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_B = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k - 1 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_F = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_FF = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + 1 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            const int idx_node_FFF = (i + d_num_diff_ghosts[0]) +
                                (j + d_num_diff_ghosts[1])*diff_ghostcell_dims[0] +
                                (k + 2 + d_num_diff_ghosts[2])*diff_ghostcell_dims[0]*
                                    diff_ghostcell_dims[1];
                            
                            diffusive_flux->getPointer(2, ei)[idx_face_z] += dt*mu[idx_diffusivity]*(
                                1.0/60*(dudz[idx_node_BBB] + dudz[idx_node_FFF])
                                - 2.0/15.0*(dudz[idx_node_BB] + dudz[idx_node_FF])
                                + 37.0/60.0*(dudz[idx_node_B] + dudz[idx_node_F]));
                        }
                    }
                }
            }
        }
        
        derivative_var_data_x.clear();
        derivative_var_data_y.clear();
        derivative_var_data_z.clear();
        
        diffusivities_data_x.clear();
        diffusivities_data_y.clear();
        diffusivities_data_z.clear();
        
        derivative_var_component_idx_x.clear();
        derivative_var_component_idx_y.clear();
        derivative_var_component_idx_z.clear();
        
        diffusivities_component_idx_x.clear();
        diffusivities_component_idx_y.clear();
        diffusivities_component_idx_z.clear();
        
        derivative_x.clear();
        derivative_y.clear();
        derivative_z.clear();
        
        /*
         * Unregister the patch and data of all registered derived cell variables in the flow model.
         */
        
        d_flow_model->unregisterPatch();
        
    } // if (d_dim == tbox::Dimension(3))
}