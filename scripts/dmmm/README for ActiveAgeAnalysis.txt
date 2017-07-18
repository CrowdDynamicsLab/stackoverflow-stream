To run: python ActiveAgeAnalysis.py

This code generates three graphs for each stack exchange:
<stack exchange>_Active_Distribution.png
<stack exchange>_Active_Proportions.png
<stack exchange>_Alive.png

Now I will introduce what these graphs mean.
Our goal is to analyze how users behave as time goes in a stack exchange.
To better analyze the status of users, it’s helpful to have the idea of “death months”.

<stack exchange>_Active_Distribution.png

This graph analyzes Number of Users vs. Active Age, which gives us a very general overview about the activeness of users in this stack exchange.


<stack exchange>_Active_Proportions.png

The proportion here means the proportion of how many months are considered active over all the active months. Say if I set my death months to be 3, and a user is active in month 4, 8, 9, 10, then his or her proportion is 1/4, where month 4 is considered active and month 8, 9, 10 don’t count. 

This 3d graph analyzes, as death months increase, how the number of users at each proportion changes. This graph is important since when we set the death months, we want to make sure they don’t cut off many users’ active age. In other cases when we do have to cut a lot, we also have to be very careful about how the proportion will change dramatically and possibly determine the reasons behind it.


<stack exchange>_Alive.png

This graph analyzes how the number of users alive changes as we increase the death months. It helps us understand if the target stack exchange owns lots of loyal users who are active all the time, or only users who just go here to enjoy one-time service at the target stack exchange.

This graph helps us determine the proper death months to choose heuristically.
