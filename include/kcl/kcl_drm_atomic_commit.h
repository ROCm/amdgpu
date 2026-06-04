/* SPDX-License-Identifier: MIT */
#ifndef AMDKCL_DRM_ATOMIC_COMMIT_H
#define AMDKCL_DRM_ATOMIC_COMMIT_H

#include <drm/drm_atomic.h>

/*
 * drm: Rename struct drm_atomic_state to drm_atomic_commit
 *
 * On older kernels, struct drm_atomic_commit does not exist.
 * Provide compat defines so code using the new name compiles on old kernels.
 *
 * The bare #define also replaces drm_atomic_commit(state) function calls
 * into drm_atomic_state(state). We fix this by:
 * 1. Saving the real function before the macro via __kcl_drm_atomic_commit()
 * 2. Adding a function-like macro drm_atomic_state(s) to redirect back.
 *    Function-like macros only match tokens followed by '(', so
 *    "struct drm_atomic_state" is NOT affected.
 */
#ifndef HAVE_STRUCT_DRM_ATOMIC_COMMIT

/* Save the real drm_atomic_commit() function before the object-like macro */
static inline int __kcl_drm_atomic_commit(struct drm_atomic_state *state)
{
	return drm_atomic_commit(state);
}

#define drm_atomic_commit drm_atomic_state
#define drm_atomic_commit_alloc drm_atomic_state_alloc
#define drm_atomic_commit_put drm_atomic_state_put
#define drm_atomic_commit_clear drm_atomic_state_clear
#define drm_atomic_commit_default_clear drm_atomic_state_default_clear
#define drm_atomic_commit_default_release drm_atomic_state_default_release

/* Restore the function: drm_atomic_commit(s) -> drm_atomic_state(s) -> __kcl_drm_atomic_commit(s) */
#define drm_atomic_state(s) __kcl_drm_atomic_commit(s)

#endif

#endif /* AMDKCL_DRM_ATOMIC_COMMIT_H */
