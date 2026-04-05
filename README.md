# MOBA-MMO 🎮

A hobby **MOBA-MMO** (Multiplayer Online Battle Arena – Massively Multiplayer Online) game project developed by a 4-person team.

---

## 🛠️ Technologies Used

| Technology | Version |
|---|---|
| Unreal Engine | 5.7 |
| Version Control | Git + Git LFS |
| Repository | GitHub |

---

## 🚀 Getting Started

### Prerequisites

Make sure you have the following installed on your machine **before cloning the repo**:

1. **Git** – [https://git-scm.com/downloads](https://git-scm.com/downloads)
2. **Git LFS** – [https://git-lfs.com](https://git-lfs.com)
3. **Unreal Engine 5.7** – via the Epic Games Launcher

---

### Initial Setup (Do This Once)

Open a terminal (or **Git Bash** on Windows) in the folder where you want to place the project, then run the following commands in order:

**Step 1 – Activate Git LFS on your machine:**
```bash
git lfs install
```
You should see: `Git LFS initialized.`

**Step 2 – Clone the repository:**
```bash
git clone https://github.com/TheMimet/MOBA-MMO.git
```

**Step 3 – Enter the project folder and pull LFS files:**
```bash
cd MOBA-MMO
git lfs pull
```

**Step 4 – Open the project:**
Double-click the `.uproject` file inside the folder to open it with Unreal Engine 5.7.

---

## 🔄 Daily Git Workflow

Follow this routine **every time** you sit down to work or finish a session to avoid merge conflicts.

### ✅ Before You Start Working (Get the Latest Changes)

Always pull the latest changes **before** opening Unreal Engine:

```bash
git pull
```

### 🚢 When You're Done Working (Send Your Changes)

After saving your work in Unreal, run these three commands in order:

```bash
# 1. Stage all your changes
git add .

# 2. Write a short message describing what you did
git commit -m "Short description of your change"

# 3. Push your changes to GitHub
git push
```

---

## ⚠️ Important Rules – Please Read!

> **Unreal Engine binary files (`.umap`, `.uasset`, Blueprints) cannot be automatically merged like regular code.**
> If two people edit the same file at the same time, one person's work **will be lost**.

### 🚫 Do NOT edit the same `.umap` or Blueprint file at the same time!

- Always announce in the group chat **which map or Blueprint you are about to work on** before opening it.
- Wait for confirmation that nobody else is currently editing that file.
- Only one person should work on a given `.umap` or Blueprint at any one time.

---

## 👥 Team

4-person hobby development team.
