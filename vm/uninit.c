/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
	/* prj3 - Anonymous Page, yeopto */
	if (page->frame != NULL) {
		// palloc_free_page(page->frame->kva); // pml4 destroy에서 알아서 해줌
		ft_delete(page->frame);
		free(page->frame);
	}

	/* Jack */
	// VM_FILE인 경우에도 aux free 하도록 수정
	if (uninit->aux != NULL) { // debugging sanori - 음... initialize 안되었지만 lazy load 되려는 애들은 free 해줘야될거같은데... 왜 안될까.......
		enum vm_type main_type = VM_TYPE(uninit->type);
		enum vm_type sub_type = VM_SUBTYPE(uninit->type);
		if (main_type == VM_ANON && sub_type == VM_SEGMENT)
			free(uninit->aux);
		else if (main_type == VM_FILE)
		{
			struct file_page *fpage = uninit->aux;
			uint32_t *oc = fpage->open_count;
			if (--(*oc) == 0)
			{
				file_close(fpage->m_file);
				free(oc);
			}
			free(uninit->aux);
		}
	}
}
