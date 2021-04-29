LIBS=include/libcanard include/o1heap
LIBCANARD_PATH=include/libcanard
O1HEAP_PATH=include/o1heap
INCLUDE_PATH=include/

# for reference
# gcc -Iinclude/ test_canard_rx.c include/libcanard/canard.c include/o1heap/o1heap.c -o test_canard_rx

output:
	rm -rf bin
	mkdir bin
	gcc -I$(INCLUDE_PATH) test_canard_rx.c $(LIBCANARD_PATH)/canard.c $(O1HEAP_PATH)/o1heap.c -o bin/test_canard_rx
	gcc -I$(INCLUDE_PATH) -pthread test_canard_tx.c $(LIBCANARD_PATH)/canard.c $(O1HEAP_PATH)/o1heap.c -o bin/test_canard_tx

clean: 
	rm -rf bin/
