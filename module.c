#include <linux/fs.h>
2	#include <linux/socket.h>
3	#include <linux/module.h>
4	#include <linux/sched.h>
5	#include <linux/slab.h>
6	#include <linux/list.h>
7	#include <linux/yura.h>
8	#include <linux/proc_fs.h>
9	#include <linux/seq_file.h>
10	
11	MODULE_LICENSE("GPL");
12	
13	struct proc_dir_entry *proc_file;
14	const char *proc_entry = "socketpair_hook_log";
15	
16	
17	/* Лист дескрипторов */
18	struct data_socket
19	{
20	    int fd1, fd2;
21	    int syscall_number;
22	    struct list_head list;
23	};
24	
25	/* Лист данных процесса */
26	struct data_process
27	{
28	    struct data_socket socket_list;
29	    pid_t proc_pid;
30	    int count;
31	    struct list_head list;
32	};
33	
34	int GLOBAL_COUNT = 1;
35	static int counter = 0;
36	static int print_counter = 0;
37	LIST_HEAD(list);
38	
39	/* Очистка листа */
40	void CleanList(void)
41	{
42	    struct list_head *iter, *iter_safe;
43	    struct data_process *temp;
44	
45	    list_for_each_safe(iter, iter_safe, &list)
46	    {
47	        temp = list_entry(iter, struct data_process, list);
48	        list_del(iter);
49	        kfree(temp);
50	    }
51	   
52	    counter = 0;
53	}
54	
55	//----------------------------------------------------------------------------------------
56	/* Блок записи в procfs */
57	static void *socket_seq_start(struct seq_file *seq, loff_t *pos)
58	{
59	  static struct list_head *iter = &list;
60	 
61	  if (list_empty(&list) || print_counter == 0)
62	  {
63	    print_counter = counter; 
64	    return NULL;
65	  }
66	 
67	  return iter->next;
68	}
69	
70	static void *socket_seq_next(struct seq_file *seq, void *v, loff_t *pos)
71	{
72	  struct list_head *iter = (struct list_head *)v;
73	 
74	  if (list_empty(iter) || print_counter == 0)
75	    return NULL;
76	 
77	  iter = iter->next;
78	  return iter;
79	}
80	
81	static int socket_m__seq_show(struct seq_file *seq, void *v)
82	{
83	  struct list_head *temp_list = (struct list_head *)v; 
84	  struct data_process *dproc = list_entry(temp_list, struct data_process, list);
85	  struct list_head *iter;
86	  struct data_socket *dsock;
87	 
88	  seq_printf(seq, "%d %d ", dproc->proc_pid, dproc->count);
89	  list_for_each(iter, &dproc->socket_list.list)
90	  {
91	    dsock = list_entry(iter, struct data_socket, list);
92	    seq_printf(seq, "%d %d %d ",dsock->syscall_number, dsock->fd1, dsock->fd2);
93	  }
94	  print_counter  = print_counter - 1;
95	  seq_printf(seq, "\n");
96	 
97	  return 0;
98	}
99	
100	static void socket_seq_stop(struct seq_file *seq, void *v)
101	{
102	}
103	
104	static struct seq_operations socket_seq_ops = {
105	  .start = socket_seq_start,
106	  .next = socket_seq_next,
107	  .stop = socket_seq_stop,
108	  .show = socket_m__seq_show,
109	};
110	
111	static int socket_seq_open(struct inode *inode, struct file *file)
112	{
113	  return seq_open(file, &socket_seq_ops);
114	}
115	
116	static struct file_operations socket_proc_fops = {
117	  .owner = THIS_MODULE,
118	  .open = socket_seq_open,
119	  .read = seq_read,
120	  .llseek = seq_lseek,
121	  .release = seq_release,
122	};
123	/* Конец блока записи*/
124	//----------------------------------------------------------------------------------------
125	
126	/*Вывод данных в лог */
127	void PrintLog(char *str)
128	{
129	    struct list_head *iter, *iter_socket;
130	    struct data_process *temp;
131	    struct data_socket *dsock;
132	
133	    printk(KERN_INFO "%s\n", str);
134	    list_for_each(iter, &list)
135	    {
136	        temp = list_entry(iter, struct data_process, list);
137	    printk(KERN_INFO "PID=%d; COUNT_OPEN_SOCKET=%d\n", temp->proc_pid, temp->count);
138	   
139	    list_for_each(iter_socket, &temp->socket_list.list)
140	    {
141	      dsock = list_entry(iter_socket, struct data_socket, list);
142	      printk(KERN_INFO "Descr. 1 = %d\n", dsock->fd1);
143	      printk(KERN_INFO "Descr. 2 = %d\n", dsock->fd2);
144	      printk(KERN_INFO "Syscall number = %d\n", dsock->syscall_number);
145	    }       
146	    }
147	    printk(KERN_INFO "============================================\n");
148	}
149	
150	
151	/* Проверка наличия процесса с открытым сокетом в списке */
152	struct list_head *FindByPid(pid_t pid)
153	{
154	    struct list_head *iter;
155	    struct data_process *temp;
156	
157	    list_for_each(iter, &list)
158	    {
159	        temp = list_entry(iter, struct data_process, list);
160	
161	        if (temp->proc_pid == pid)
162	            return iter;
163	    }
164	
165	    return NULL;
166	}
167	
168	/* Обработчик socketpair */
169	void SocketPair_hook(int fd1, int fd2)
170	{
171	    struct list_head *temp = FindByPid(current->pid);
172	    struct data_process *data;
173	    struct data_socket *dsock;
174	
175	    if (temp == NULL)
176	    {
177	    counter = counter + 1;
178	    print_counter = counter;
179	        data = (struct data_process *)kmalloc(sizeof(struct data_process), GFP_KERNEL);
180	   
181	    INIT_LIST_HEAD(&data->socket_list.list);
182	    dsock = (struct data_socket *)kmalloc(sizeof(struct data_socket), GFP_KERNEL);
183	    dsock->fd1 = fd1;
184	    dsock->fd2 = fd2;
185	    dsock->syscall_number = GLOBAL_COUNT;
186	    GLOBAL_COUNT = GLOBAL_COUNT + 1;
187	    list_add(&(dsock->list), &data->socket_list.list);
188	   
189	        data->count = 2;
190	        data->proc_pid = current->pid;
191	        list_add(&(data->list), &list);
192	    }
193	    else
194	    {
195	        dsock = (struct data_socket *)kmalloc(sizeof(struct data_socket), GFP_KERNEL);
196	    dsock->fd1 = fd1;
197	    dsock->fd2 = fd2;
198	    dsock->syscall_number = GLOBAL_COUNT;
199	    GLOBAL_COUNT = GLOBAL_COUNT + 1;
200	        data = list_entry(temp, struct data_process, list);
201	    list_add(&(dsock->list), &data->socket_list.list);
202	        (*data).count += 2;
203	    }
204	
205	    PrintLog("Socketpair_Log");
206	}
207	
208	/* Обработчки clone */
209	void Clone_hook(const unsigned long ret)
210	{
211	    struct list_head *temp = FindByPid(current->pid);
212	    struct data_process *data_pred, *data;
213	    struct data_socket *dsock_pred, *dsock;
214	    struct list_head *iter;
215	   
216	    if (temp != NULL)
217	    {
218	    counter = counter + 1;
219	    print_counter = counter;
220	        data = (struct data_process *)kmalloc(sizeof(struct data_process), GFP_KERNEL);
221	        data_pred = list_entry(temp, struct data_process, list);
222	        data->count = data_pred->count;
223	   
224	    INIT_LIST_HEAD(&data->socket_list.list);
225	    list_for_each(iter, &data_pred->socket_list.list)
226	    {
227	      dsock_pred = (struct data_socket *)kmalloc(sizeof(struct data_socket), GFP_KERNEL);
228	      dsock = (struct data_socket *)kmalloc(sizeof(struct data_socket), GFP_KERNEL);
229	      dsock_pred = list_entry(iter, struct data_socket, list);
230	      dsock->fd1 = dsock_pred->fd1;
231	      dsock->fd2 = dsock_pred->fd2;
232	      dsock->syscall_number = dsock_pred->syscall_number;
233	      list_add(&(dsock->list), &data->socket_list.list);
234	    }
235	   
236	    data->proc_pid = ret;
237	    list_add(&(data->list), &list);
238	        PrintLog("Clone_Log");
239	    } 
240	 }
241	 
242	 /*Обработчик close*/
243	 void Close_hook(unsigned int fd)
244	 {
245	   struct data_process *data;
246	   struct data_socket *dsock;
247	   struct list_head *iter;
248	   struct list_head *temp = FindByPid(current->pid);
249	   
250	   if (temp != NULL)
251	   {
252	     data = list_entry(temp, struct data_process, list);
253	     list_for_each(iter, &data->socket_list.list)
254	     {
255	       dsock = (struct data_socket *)kmalloc(sizeof(struct data_socket), GFP_KERNEL);
256	       dsock = list_entry(iter, struct data_socket, list);
257	       if (dsock->fd1 == fd)
258	       {
259	     data->count = data->count - 1;
260	     dsock->fd1 = -1;
261	       }
262	       if (dsock->fd2 == fd)
263	       {
264	     data->count = data->count - 1;
265	     dsock->fd2 = -1;
266	       }
267	     }     
268	     PrintLog("Close_Log");
269	   }
270	 }
271	
272	
273	/* Инициализация модуля */
274	int init_module_sp(void)
275	{
276	    int rv = 0;
277	    proc_file = create_proc_entry(proc_entry, 0644, NULL);
278	
279	    if (proc_file == NULL)
280	    {
281	        rv = -ENOMEM;
282	        goto error;
283	    }
284	   
285	    proc_file->proc_fops = &socket_proc_fops;
286	    printk(KERN_INFO "%s was created\n", proc_entry);
287	   
288	    set_socketpair_hook(SocketPair_hook);
289	    set_clone_hook(Clone_hook);
290	    set_close_hook(Close_hook);
291	
292	error:
293	    return rv;
294	}
295	
296	
297	/* Завершение работы модуля */
298	void exit_module_sp(void)
299	{
300	    set_socketpair_hook(NULL);
301	    set_clone_hook(NULL);
302	    set_close_hook(NULL);
303	
304	    CleanList();
305	    remove_proc_entry(proc_entry, proc_file);
306	    printk(KERN_INFO "%s removed\n", proc_entry);
307	}
308	
309	module_init(init_module_sp);
310	module_exit(exit_module_sp);
