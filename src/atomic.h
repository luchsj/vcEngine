//atomic operations on 32-bit integers

//increment a number atomically
//returns previous value of number
//performs following operation atomically:
//	int old_val = *address; (*address)++; return old_value;
int atomic_increment(int* address);

//decrement a number atomically
//returns previous value of number
//performs following operation atomically:
//	int old_val = *address; (*address)--; return old_value;
int atomic_decrement(int* address);

//compare two numbers atomically and assign if equal
//returns old value of number
//performs following operation atomically:
//	int old_value = *address; if (*address == compare) *address = exchange;
int atomic_compare_and_exchange(int* dest, int compare, int exchange);

//reads integer from address
//all writes before last atomic_store to this address are flushed
int atomic_load(int* address);

//writes an integer
//paired with an atomic_load, can guarantee ordering and visibility
void atomic_store(int* address, int value);
