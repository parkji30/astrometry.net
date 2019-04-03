/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */
#include <stdio.h>
#include <assert.h>

#include "cutest.h"
#include "index.h"
#include "engine.h"
#include "solver.h"
#include "xylist.h"
#include "bl.h"
#include "log.h"
#include "errors.h"

/*

 - build-astrometry-index -d 3 -o index-9918.fits -P 18 -S mag -B 0.1 -s 0 -r 1 -I 9918 -M -i demo/tycho2-mag6.fits
 - echo -e 'add_path .\ninparallel\nindex index-9918.fits' > 99.cfg
 - solve-field --config 99.cfg demo/apod4.jpg  --continue --keep-xylist apod4.xy
 - solve-field --config 99.cfg --continue apod4.xy --width 719 --height 507

 */

void test_predistort(CuTest* ct) {
    // core Astrometry solver parameters
    solver_t* solver;
    int imagew, imageh;
    double imagecx, imagecy;
    double deg_width_min = 30;
    double deg_width_max = 40;

    char* xyfn = "apod4.xy";
    char* indexfn = "index-9918.fits";

    int loglvl = LOG_MSG;

    loglvl++;
    log_init(loglvl);
    
    imagew = 719;
    imageh = 507;

    imagecx = (imagew - 1.0)/2.0;
    imagecy = (imageh - 1.0)/2.0;
    
    // Here we initialize the core astrometry solver struct, telling
    // it about the possible range of image scales.
    solver = solver_new();
    double qsf_min = 0.1;
    // don't try teeny-tiny quads.
    solver->quadsize_min = qsf_min * MIN(imagew, imageh);

    // compute scale range in arcseconds per pixel.
    // set the solver's "funits" = field (image) scale units
    solver->funits_lower = 3600. * deg_width_min / (double)imagew;
    solver->funits_upper = 3600. * deg_width_max / (double)imagew;
    
    solver_set_keep_logodds(solver, log(1e12));

    xylist_t* xyls = xylist_open(xyfn);
    starxy_t* xy = xylist_read_field(xyls, NULL);
    // Feed the image source coordinates to the solver...
    //starxy_set_flux_array(field, starflux);
    //starxy_sort_by_flux(field);
    solver_set_field(solver, xy);
    solver_set_field_bounds(solver, 0, imagew, 0, imageh);

    index_t* index = index_load(indexfn, 0, NULL);
    solver_add_index(solver, index);
    solver->distance_from_quad_bonus = TRUE;
    solver_run(solver);

    assert(solver->best_match_solves);
    double ra, dec;
    double pscale;
    tan_t* wcs;
    logmsg("Solved using index %s with odds ratio %g\n",
           solver->best_index->indexname,
           solver->best_match.logodds);
    // WCS is solver->best_match.wcstan
    wcs = &(solver->best_match.wcstan);
    // center
    tan_pixelxy2radec(wcs, imagecx, imagecy, &ra, &dec);
    pscale = tan_pixel_scale(wcs);
    logmsg("Image center is RA,Dec = (%g,%g) degrees, size is %.2g x %.2g arcmin.\n",
           ra, dec, arcsec2arcmin(pscale * imagew), arcsec2arcmin(pscale * imageh));


    //solver_cleanup_field(solver);
    solver_reset_best_match(solver);
    solver_reset_counters(solver);

    // solver->fieldxy->{N,x,y};

    /*
     double coeff = 1e-3;
     N = solver->fieldxy->N;
     for (i=0; i<N; i++) {
     }
     */

    sip_t distortion;
    memset(&distortion, 0, sizeof(sip_t));
    distortion.wcstan.imagew = imagew;
    distortion.wcstan.imageh = imageh;
    distortion.wcstan.crpix[0] = imagecx;
    distortion.wcstan.crpix[1] = imagecy;
    distortion.a_order = 2;
    distortion.b_order = 2;
    distortion.a[2][0] = 1e-4;

    // Compute distorted star positions
    starxy_t* xy_dist;
    int i,N;
    N = xy->N;
    xy_dist = starxy_new(N, FALSE, FALSE);
    for (i=0; i<N; i++) {
        double dx,dy;
        sip_pixel_distortion(&distortion, xy->x[i], xy->y[i], &dx, &dy);
        xy_dist->x[i] = dx;
        xy_dist->y[i] = dy;
        printf("x,y %.1f, %.1f -> %.1f, %.1f (delta %.1f, %.1f)\n",
               xy->x[i], xy->y[i], dx, dy, dx - xy->x[i], dy - xy->y[i]);
    }

    // avoid solver freeing "xy".
    //solver->fieldxy = NULL;
    
    solver_set_field(solver, xy_dist);
    solver_set_field_bounds(solver, 0, imagew, 0, imageh);

    solver_run(solver);
    assert(solver->best_match_solves);

    logmsg("Solved using index %s with odds ratio %g\n",
           solver->best_index->indexname,
           solver->best_match.logodds);
    // WCS is solver->best_match.wcstan
    wcs = &(solver->best_match.wcstan);
    // center
    tan_pixelxy2radec(wcs, imagecx, imagecy, &ra, &dec);
    pscale = tan_pixel_scale(wcs);
    logmsg("Image center is RA,Dec = (%g,%g) degrees, size is %.2g x %.2g arcmin.\n",
           ra, dec, arcsec2arcmin(pscale * imagew), arcsec2arcmin(pscale * imageh));
    
}

