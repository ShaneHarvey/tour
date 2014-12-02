#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cache.h"
#include "minunit.h"
#include "debug.h"

static Cache *list = NULL;
int tests_run = 0;
#define TOTAL_TESTS 3

static void init_cache(Cache *cache, int value) {
    cache->sll_ifindex = value;
    cache->domain_socket = value;
    cache->sll_hatype = value;
    memset(cache->if_haddr, value, IFHWADDRLEN);
}

static char* test_create_cache(void) {
    Cache *node = NULL;
    size_t count = 0;
    /* Allocate space for the cache */
    Cache *c1 = malloc(sizeof(Cache));
    Cache *c2 = malloc(sizeof(Cache));
    Cache *c3 = malloc(sizeof(Cache));
    Cache *c4 = malloc(sizeof(Cache));
    /* memset the structs */
    memset(c1, 0, sizeof(Cache));
    memset(c2, 0, sizeof(Cache));
    memset(c3, 0, sizeof(Cache));
    memset(c4, 0, sizeof(Cache));
    /* Set some random fields of c1, c2, c3, and c4 */
    init_cache(c1, 1);
    init_cache(c2, 2);
    init_cache(c3, 3);
    init_cache(c4, 4);
    /* Now add it to the list */
    mu_assert("error, unable to add c1", addToCache(&list, c1) == true);
    mu_assert("error, unable to add c2", addToCache(&list, c2) == true);
    mu_assert("error, unable to add c3", addToCache(&list, c3) == true);
    mu_assert("error, unable to add c4", addToCache(&list, c4) == true);
    /* Add incorrect entry to the list (should fail) */
    mu_assert("error, added a NULL node", addToCache(&list, NULL) == false);
    mu_assert("error, added to a NULL list", addToCache(NULL, c1) == false);
    mu_assert("error, added a NULL node to a NULL list", addToCache(NULL, NULL) == false);
    /* Now check the size of the list */
    node = list;
    while(node != NULL) {
        count++;
        node = node->next;
    }
    /* Check to make sure the correct number of nodes were added */
    mu_assert("error, the size of the list is not equal to the cache entries added", count == 4);
    /* On success return 0 */
    return EXIT_SUCCESS;
}

static char* test_update_cache(void) {
    /* Allocate space for a new node */
    Cache *node = malloc(sizeof(Cache));
    /* Copy the first node in the list */
    memcpy(node, list, sizeof(Cache));
    /* Change a value in the node */
    node->domain_socket = 1337;
    /* Attempt to update the node */
    mu_assert("Failed to update an existing cache entry.", updateCache(list, node) == true);
    /* Check that the value of the head node is actually changed */
    mu_assert("Update function failed to update the cache entry.", node->domain_socket == list->domain_socket);
    /* Attempt to update a cache entry that doesn't exist */
    init_cache(node, 18);
    mu_assert("Updated a cache entry that does not exist.", updateCache(list, node) == false);
    /* Attempt to update with NULL entries */
    mu_assert("Updated a cache entry in a NULL list.", updateCache(NULL, node) == false);
    mu_assert("Updated a NULL cache entry in the list.", updateCache(list, NULL) == false);
    mu_assert("Updated a NULL cache entry in a NULL list.", updateCache(NULL, NULL) == false);
    return EXIT_SUCCESS;
}

static char* test_samecache(void) {
    Cache c1;
    Cache c2;
    /* Initialize the first cache */
    init_cache(&c1, 554);
    /* Copy the first cache into the second cache */
    memcpy(&c2, &c1, sizeof(Cache));
    /* Test to see if same cache */
    mu_assert("The same cache is not equal to itself.", isSameCache(&c1, &c1) == true);
    mu_assert("Two equal caches are not the same.", isSameCache(&c1, &c2) == true);
    /* Check two different caches */
    c2.sll_ifindex = 1984;
    mu_assert("Two differnet caches say they are the same.", isSameCache(&c1, &c2) == false);
    /* Check NULL entries */
    mu_assert("A NULL Cache is equal to an existing cache.", isSameCache(&c1, NULL) == false);
    mu_assert("A NULL Cache is equal to an existing cache.", isSameCache(NULL, &c2) == false);
    mu_assert("Two NULL caches are equal.", isSameCache(NULL, NULL) == false);
    return EXIT_SUCCESS;
}

static char* all_tests() {
    mu_run_test(test_create_cache);
    mu_run_test(test_samecache);
    mu_run_test(test_update_cache);
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    char *result = all_tests();
    if (result != 0) {
        error("%s\n", result);
    } else {
        success("ALL TESTS PASSED\n");
    }
    info("Tests run: %d/%d\n", tests_run, TOTAL_TESTS);
    return result != 0;
}
