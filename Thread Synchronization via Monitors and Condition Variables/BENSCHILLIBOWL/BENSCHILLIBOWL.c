#include "BENSCHILLIBOWL.h"
#include <assert.h>
#include <time.h>
#include <stdio.h>
#include <pthread.h>

static MenuItem BENSCHILLIBOWLMenu[] = { 
    "BensChilli", 
    "BensHalfSmoke", 
    "BensHotDog", 
    "BensChilliCheeseFries", 
    "BensShake",
    "BensHotCakes",
    "BensCake",
    "BensHamburger",
    "BensVeggieBurger",
    "BensOnionRings",
};
static int BENSCHILLIBOWLMenuLength = 10;

// Forward declarations of helper functions
bool IsEmpty(BENSCHILLIBOWL* bcb);
bool IsFull(BENSCHILLIBOWL* bcb);
void AddOrderToBack(Order **orders, Order *order);

/* Select a random item from the Menu and return it */
MenuItem PickRandomMenuItem() {
    int index = rand() % BENSCHILLIBOWLMenuLength;
    return BENSCHILLIBOWLMenu[index];
}

/* Allocate memory for the Restaurant, initialize it, and setup synchronization objects */
BENSCHILLIBOWL* OpenRestaurant(int max_size, int expected_num_orders) {
    BENSCHILLIBOWL* bcb = (BENSCHILLIBOWL*) malloc(sizeof(BENSCHILLIBOWL));
    bcb->orders = NULL;
    bcb->current_size = 0;
    bcb->max_size = max_size;
    bcb->next_order_number = 1;
    bcb->orders_handled = 0;
    bcb->expected_num_orders = expected_num_orders;

    pthread_mutex_init(&(bcb->mutex), NULL);
    pthread_cond_init(&(bcb->can_add_orders), NULL);
    pthread_cond_init(&(bcb->can_get_orders), NULL);

    srand(time(NULL));
    printf("Restaurant is open!\n");
    return bcb;
}

/* Close the restaurant:
   - Ensure all orders handled == expected number of orders
   - Destroy synchronization objects
   - Free restaurant memory */
void CloseRestaurant(BENSCHILLIBOWL* mcg) {
    BENSCHILLIBOWL *bcb = mcg;
    assert(bcb->orders_handled == bcb->expected_num_orders);

    pthread_mutex_destroy(&(bcb->mutex));
    pthread_cond_destroy(&(bcb->can_add_orders));
    pthread_cond_destroy(&(bcb->can_get_orders));

    free(bcb);
    printf("Restaurant is closed!\n");
}

/* Add an order:
   - Wait if restaurant is full
   - Add order to the back of the queue
   - Set order_number
   - Signal that orders are available */
int AddOrder(BENSCHILLIBOWL* mcg, Order* order) {
    BENSCHILLIBOWL *bcb = mcg;
    pthread_mutex_lock(&(bcb->mutex));

    while (IsFull(bcb)) {
        pthread_cond_wait(&(bcb->can_add_orders), &(bcb->mutex));
    }

    order->order_number = bcb->next_order_number++;
    order->next = NULL;
    AddOrderToBack(&(bcb->orders), order);
    bcb->current_size++;

    pthread_cond_signal(&(bcb->can_get_orders));
    pthread_mutex_unlock(&(bcb->mutex));

    return order->order_number;
}

/* Get an order:
   - Wait if empty and not done yet
   - If done (all orders handled), return NULL
   - Otherwise, remove from the front and return order */
Order *GetOrder(BENSCHILLIBOWL* mcg) {
    BENSCHILLIBOWL *bcb = mcg;
    pthread_mutex_lock(&(bcb->mutex));

    // If all orders are already handled, return NULL
    if (bcb->orders_handled == bcb->expected_num_orders) {
        pthread_mutex_unlock(&(bcb->mutex));
        return NULL;
    }

    while (IsEmpty(bcb) && bcb->orders_handled < bcb->expected_num_orders) {
        pthread_cond_wait(&(bcb->can_get_orders), &(bcb->mutex));
    }

    // Check again after waiting
    if (IsEmpty(bcb) && bcb->orders_handled == bcb->expected_num_orders) {
        pthread_mutex_unlock(&(bcb->mutex));
        return NULL;
    }

    // Remove from front
    Order* order = bcb->orders;
    bcb->orders = order->next;
    bcb->current_size--;
    bcb->orders_handled++;

    // Signal that space is now available
    pthread_cond_signal(&(bcb->can_add_orders));
    pthread_mutex_unlock(&(bcb->mutex));

    return order;
}

// Helper functions
bool IsEmpty(BENSCHILLIBOWL* bcb) {
    return (bcb->current_size == 0);
}

bool IsFull(BENSCHILLIBOWL* bcb) {
    return (bcb->current_size == bcb->max_size);
}

void AddOrderToBack(Order **orders, Order *order) {
    if (*orders == NULL) {
        *orders = order;
        return;
    }
    Order *temp = *orders;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = order;
}
