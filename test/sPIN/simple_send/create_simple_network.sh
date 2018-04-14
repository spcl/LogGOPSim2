#!/bin/bash
#The script creates a simple network with a single switch
#Topology: 1 switch n hosts 


size=${1}
 

OUTPUT="simplenet.dot"

echo "digraph mycoolnetwork {"> ${OUTPUT}

for (( i=0; i<size;i++))
do
    table="";
    for (( j=0; j<size;j++))
    do
        if [ $j -ne $i ]; then
            if [ "x" == "x${table}" ]; then
                table="H${j}"
            else
                table="$table,H${j}"
            fi
        fi
    done


echo "    H${i} -> S1 [comment=\""${table}"\"];" >> ${OUTPUT}
done

for (( i=0; i<size;i++))
do
echo "    S1 -> H${i} [comment=\""H${i}"\"];" >> ${OUTPUT}
done

echo "}">> ${OUTPUT}
