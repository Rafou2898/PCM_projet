```cpp
	static TSPPath _shortest;
	static std::vector<TSPTask*> _free_list;
 ```
ceux la ca va pas, il faut les proteger avec un cas. Mais cas sur un pointeur de cette structure
sinon un par thread mais bof

ca pas ouf :
```cpp
	TSPTask* reusealloc(int node) {
		if (_free_list.empty())
			return new TSPTask(this, node);
		TSPTask* p = _free_list.back();
		_free_list.pop_back();
		p->_path = _path;
		p->_cutoff_size = _cutoff_size;
		p->_path.push(node);
		return p;
        }
```
parce que c est partagé par tous les thread, donc faudrait en faire un par thread

Comment detecter qu il y a plus de tache a réaliser? Terminaison?
On connais la taille de l abre quand on commence, si on a 2 0villes, le nombre de feuilles
va etre factorielle. Donc a chaque solve, on sais ce qui reste a parcourir et combien de feuilles
le solve a visiter.
On démarre avec le nombre de feuille a visiter qu on décrémente, entier atomique qui va toujours decroitre
quand il arrive a 0 c est la fin. 



Exception pour modifier le code:
Le split verifie pas la taille du chemin, c est que le solve qui fait ca
Donc on split des chemin qui sont deja plus long. Donc faut que le split verifie la taille du chemin avant de split
