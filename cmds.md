## 下载iEDA仓库
## git switch --detach 0074352412f6a4a8c88c13739946cdf5004f25c0
git clone --recursive git@github.com:zhiyiwang/iEDA-EDADB.git
git clone --recursive git@grape:zhiyi/ieda-edadb.git

git clone --recursive git@github.com:RogRivers/iEDA_New_Edadb.git
git clone git@github.com:RogRivers/iEDA_New_Edadb.git 
git submodule update --init --recursive

## setup并测试程序
# 通过apt安装编译依赖，需要root权限
sudo bash build.sh -i apt
# 编译 iEDA
bash build.sh -j 16 2>&1 | tee build.out
# 若能够正常输出 "Hello iEDA!" 则编译成功
./bin/iEDA -script scripts/hello.tcl

## 生成design相关文件
cd bin
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/design/sky130_gcd/run_iEDA.sh
ls scripts/design/sky130_gcd/script/DB_script

## 运行iEDA+edadb
cd bin/
pwd
bash /home/zhiyiwang/cs/arch/eda/iEDA-EDADB/scripts/edadb/demo/demo.sh 2>&1 | tee run.out
